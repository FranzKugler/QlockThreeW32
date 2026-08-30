/**
 * ResidualStore
 * See ResidualStore.h for the three rules, the sphere, and why the coupling
 * to FactoryProfile is now real.
 *
 * @mc       ESP32S3
 * @author   Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
 * @version  2.4
 * @created  28.8.2026
 * @updated  30.8.2026
 */
#include "ResidualStore.h"

#include <math.h>

namespace
{
    // Not M_PI: see FactoryProfile.cpp for why (a glibc extension hidden
    // under -std=c++17 without a POSIX feature macro).
    const double PI = 3.14159265358979323846;

    /** Degrees between two hues, measured the short way round the wheel. */
    double hueDistance(double a, double b, double period)
    {
        double gap = fmod(fabs(a - b), period);
        if (gap > period / 2.0) gap = period - gap;
        return gap;
    }

    /**
     * The nose's three coefficients by ordinary least squares - a0 + a1*cos +
     * b1*sin against `targets`, one row per point. False (and `out`
     * untouched) below three points, where the system has more unknowns than
     * equations, or if the points turn out collinear in a way three of them
     * cannot be (a coincidence this small a group can hit).
     */
    bool solveNose(const double rows[][3], const double *targets, int n, double out[3])
    {
        if (n < 3) return false;

        double a[3][4] = {{0.0}};
        for (int p = 0; p < n; p++)
        {
            for (int i = 0; i < 3; i++)
            {
                a[i][3] += rows[p][i] * targets[p];
                for (int j = 0; j < 3; j++) a[i][j] += rows[p][i] * rows[p][j];
            }
        }
        for (int col = 0; col < 3; col++)
        {
            int pivot = col;
            for (int r = col + 1; r < 3; r++)
            {
                if (fabs(a[r][col]) > fabs(a[pivot][col])) pivot = r;
            }
            if (fabs(a[pivot][col]) < 1e-12) return false;
            if (pivot != col)
            {
                for (int j = 0; j < 4; j++)
                {
                    double t = a[col][j]; a[col][j] = a[pivot][j]; a[pivot][j] = t;
                }
            }
            double scale = a[col][col];
            for (int j = col; j < 4; j++) a[col][j] /= scale;
            for (int r = 0; r < 3; r++)
            {
                if (r == col) continue;
                double factor = a[r][col];
                for (int j = col; j < 4; j++) a[r][j] -= factor * a[col][j];
            }
        }
        out[0] = a[0][3]; out[1] = a[1][3]; out[2] = a[2][3];
        return true;
    }

    /**
     * A straight line by ordinary least squares. False below two points or
     * with no spread in x at all - the same "cannot be estimated" this
     * project already refuses a slope on elsewhere (Luminance's white line).
     */
    bool solveLine(const double *xs, const double *ys, int n, double &slope, double &offset)
    {
        if (n < 2) return false;
        double sumX = 0.0, sumY = 0.0;
        for (int i = 0; i < n; i++) { sumX += xs[i]; sumY += ys[i]; }
        double meanX = sumX / n, meanY = sumY / n;
        double covariance = 0.0, variance = 0.0;
        for (int i = 0; i < n; i++)
        {
            double dx = xs[i] - meanX;
            covariance += dx * (ys[i] - meanY);
            variance += dx * dx;
        }
        if (variance < 1e-12) return false;
        slope = covariance / variance;
        offset = meanY - slope * meanX;
        return true;
    }
}

uint16_t ResidualStore::canonicalHue(uint16_t hue, uint8_t sat)
{
    // Zero rather than the hue that happened to be set. At saturation zero the
    // driver emits 255,255,255 whatever the hue byte says, so keeping it would
    // make two identical statements look like two different ones - and fill
    // two of eight slots with the same fact.
    if (sat == 0) return 0;
    return (uint16_t)(hue % 360);
}

bool ResidualStore::sameColour(uint16_t hueA, uint8_t satA,
                               uint16_t hueB, uint8_t satB)
{
    if (satA != satB) return false;
    return canonicalHue(hueA, satA) == canonicalHue(hueB, satB);
}

void ResidualStore::add(Store &store, double logLux, double decades,
                        uint16_t hue, uint8_t sat, uint32_t seconds, bool bound)
{
    uint16_t canonical = canonicalHue(hue, sat);

    for (uint8_t i = 0; i < store.count; i++)
    {
        // The same colour *and* the same light. Either one different and this
        // is a new statement rather than an update of an old one.
        if (!sameColour(store.at[i].hue, store.at[i].sat, canonical, sat)) continue;
        if (fabs(logLux - store.at[i].logLux) > RESIDUAL_SAME_LUX) continue;

        // Taken out rather than overwritten in place, so what goes in below is
        // always the newest and the stored order stays the order things
        // happened - which is what decides who leaves when the store is full.
        for (uint8_t j = i; j + 1 < store.count; j++) store.at[j] = store.at[j + 1];
        store.count--;
        break;   // at most one; two would have replaced each other already
    }

    if (store.count == RESIDUAL_MAX)
    {
        for (uint8_t j = 0; j + 1 < store.count; j++) store.at[j] = store.at[j + 1];
        store.count--;
    }

    store.at[store.count].logLux = logLux;
    store.at[store.count].decades = decades;
    store.at[store.count].hue = canonical;
    store.at[store.count].sat = sat;
    store.at[store.count].seconds = seconds;
    store.at[store.count].bound = bound ? 1 : 0;
    store.count++;
}

bool ResidualStore::forget(Store &store, uint8_t index)
{
    if (index >= store.count) return false;
    for (uint8_t j = index; j + 1 < store.count; j++) store.at[j] = store.at[j + 1];
    store.count--;
    return true;
}

bool ResidualStore::refit(const FactoryProfile::Profile &factory, const Store &store,
                          Fit &out)
{
    // Seeded with the factory's own numbers first: a half that ends up with
    // too little to refit keeps exactly what the factory shipped rather than
    // a zero, which would be a much louder wrong answer.
    out.noseA0 = factory.noseA0; out.noseA1 = factory.noseA1; out.noseB1 = factory.noseB1;
    out.blueSlope = factory.blueSlope; out.blueOffset = factory.blueOffset;
    if (!FactoryProfile::valid(factory)) return false;

    // Which factory points a taught one shadows. A bound counts here even
    // though it is left out of the regression below: "at least this much" is
    // still the owner saying the factory number at that light and colour is
    // wrong, which is a different question from whether it can be read as an
    // equality.
    bool shadowed[FACTORY_MAX_POINTS] = {false};
    for (uint8_t i = 0; i < factory.pointCount; i++)
    {
        for (uint8_t j = 0; j < store.count; j++)
        {
            if (store.at[j].sat == 0) continue;   // white shadows nothing colour-shaped
            double dh = hueDistance(factory.point[i].hue, store.at[j].hue,
                                    factory.huePeriod) / RESIDUAL_SHADOW_HUE;
            double dx = (factory.point[i].logLux - store.at[j].logLux) / RESIDUAL_SHADOW_LUX;
            if (dh * dh + dx * dx < 1.0) { shadowed[i] = true; break; }
        }
    }

    // The two regressions, built from whatever survives: factory points not
    // shadowed, and taught points that are neither white nor a bound. Split
    // by the same distance blueWeight() blends over - inside the window a
    // point speaks to blue's line, outside it speaks to the nose.
    double noseRows[FACTORY_MAX_POINTS + RESIDUAL_MAX][3];
    double noseTargets[FACTORY_MAX_POINTS + RESIDUAL_MAX];
    int noseCount = 0;
    double blueXs[FACTORY_MAX_POINTS + RESIDUAL_MAX];
    double blueYs[FACTORY_MAX_POINTS + RESIDUAL_MAX];
    int blueCount = 0;

    for (uint8_t i = 0; i < factory.pointCount; i++)
    {
        if (shadowed[i]) continue;
        const FactoryProfile::Point &p = factory.point[i];
        if (hueDistance(p.hue, factory.blueHue, factory.huePeriod) < factory.blendHalfWidth)
        {
            blueXs[blueCount] = p.logLux; blueYs[blueCount] = p.residual; blueCount++;
        }
        else
        {
            double rad = p.hue * PI / 180.0;
            noseRows[noseCount][0] = 1.0; noseRows[noseCount][1] = cos(rad);
            noseRows[noseCount][2] = sin(rad); noseTargets[noseCount] = p.residual;
            noseCount++;
        }
    }
    for (uint8_t j = 0; j < store.count; j++)
    {
        const Residual &r = store.at[j];
        if (r.sat == 0 || r.bound) continue;
        if (hueDistance(r.hue, factory.blueHue, factory.huePeriod) < factory.blendHalfWidth)
        {
            blueXs[blueCount] = r.logLux; blueYs[blueCount] = r.decades; blueCount++;
        }
        else
        {
            double rad = r.hue * PI / 180.0;
            noseRows[noseCount][0] = 1.0; noseRows[noseCount][1] = cos(rad);
            noseRows[noseCount][2] = sin(rad); noseTargets[noseCount] = r.decades;
            noseCount++;
        }
    }

    double nose[3];
    if (solveNose(noseRows, noseTargets, noseCount, nose))
    {
        out.noseA0 = nose[0]; out.noseA1 = nose[1]; out.noseB1 = nose[2];
    }
    double slope, offset;
    if (solveLine(blueXs, blueYs, blueCount, slope, offset))
    {
        out.blueSlope = slope; out.blueOffset = offset;
    }
    return true;
}
