/**
 * Language_ES
 * Spanish.
 *
 * "es la una" for one o'clock but "son las dos" for every other hour - the
 * special case the old renderer kept in ES_hours(), now part of hour().
 *
 * Spanish never says "en punto" on this panel, so there is no word for the top
 * of the hour and `full` does not appear here.
 *
 * @mc       ESP32S3
 * @author   Christian Aschoff / caschoff _AT_ mac _DOT_ com (panel)
 * @author   Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
 * @version  2.1
 * @created  20.8.2026
 * @updated  20.8.2026
 */
#include "Language.h"

namespace
{
    /*
     *   01234567890
     * 0 ESONELASUNA
     * 1 DOSITRESORE
     * 2 CUATROCINCO
     * 3 SEISASIETEN
     * 4 OCHONUEVEYO
     * 5 LADIEZSONCE
     * 6 DOCELYMENOS
     * 7 OVEINTEDIEZ
     * 8 VEINTICINCO
     * 9 MEDIACUARTO
     */
    enum
    {
        ES_, SON, LA, LAS,
        Y, MENOS, CINCO, DIEZ, CUARTO, VEINTE, VEINTICINCO, MEDIA,
        H_UNA, H_DOS, H_TRES, H_CUATRO, H_CINCO, H_SEIS,
        H_SIETE, H_OCHO, H_NUEVE, H_DIEZ, H_ONCE, H_DOCE
    };

    const Word WORDS[] = {
        { 0, 0, "ES" },
        { 0, 1, "SON" },
        { 0, 5, "LA" },
        { 0, 5, "LAS" },

        { 6, 5, "Y" },
        { 6, 6, "MENOS" },
        { 8, 6, "CINCO" },
        { 7, 7, "DIEZ" },
        { 9, 5, "CUARTO" },
        { 7, 1, "VEINTE" },
        { 8, 0, "VEINTICINCO" },
        { 9, 0, "MEDIA" },

        { 0, 8, "UNA" },
        { 1, 0, "DOS" },
        { 1, 4, "TRES" },
        { 2, 0, "CUATRO" },
        { 2, 6, "CINCO" },
        { 3, 0, "SEIS" },
        { 3, 5, "SIETE" },
        { 4, 0, "OCHO" },
        { 4, 4, "NUEVE" },
        { 5, 2, "DIEZ" },
        { 5, 7, "ONCE" },
        { 6, 0, "DOCE" }
    };

    /** The hour, and the "es la" / "son las" that agrees with it. */
    void hour(int8_t hours, Face &face)
    {
        while (hours < 0) hours += 12;
        while (hours > 24) hours -= 12;

        if (hours == 1 || hours == 13)
        {
            face.light(ES_);
            face.light(LA);
        }
        else
        {
            face.light(SON);
            face.light(LAS);
        }

        switch (hours % 12)
        {
            case 0:  face.light(H_DOCE); break;
            case 1:  face.light(H_UNA); break;
            case 2:  face.light(H_DOS); break;
            case 3:  face.light(H_TRES); break;
            case 4:  face.light(H_CUATRO); break;
            case 5:  face.light(H_CINCO); break;
            case 6:  face.light(H_SEIS); break;
            case 7:  face.light(H_SIETE); break;
            case 8:  face.light(H_OCHO); break;
            case 9:  face.light(H_NUEVE); break;
            case 10: face.light(H_DIEZ); break;
            case 11: face.light(H_ONCE); break;
        }
    }

    void render(int8_t hours, uint8_t minutes, Face &face)
    {
        switch (minutes / 5)
        {
            case 0:
                hour(hours, face);
                break;
            case 1:
                face.light(Y); face.light(CINCO);
                hour(hours, face);
                break;
            case 2:
                face.light(Y); face.light(DIEZ);
                hour(hours, face);
                break;
            case 3:
                face.light(Y); face.light(CUARTO);
                hour(hours, face);
                break;
            case 4:
                face.light(Y); face.light(VEINTE);
                hour(hours, face);
                break;
            case 5:
                face.light(Y); face.light(VEINTICINCO);
                hour(hours, face);
                break;
            case 6:
                face.light(Y); face.light(MEDIA);
                hour(hours, face);
                break;
            case 7:
                face.light(MENOS); face.light(VEINTICINCO);
                hour(hours + 1, face);
                break;
            case 8:
                face.light(MENOS); face.light(VEINTE);
                hour(hours + 1, face);
                break;
            case 9:
                face.light(MENOS); face.light(CUARTO);
                hour(hours + 1, face);
                break;
            case 10:
                face.light(MENOS); face.light(DIEZ);
                hour(hours + 1, face);
                break;
            case 11:
                face.light(MENOS); face.light(CINCO);
                hour(hours + 1, face);
                break;
        }
    }
}

extern const Language LANGUAGE_SPANISH = {
    "es", "Español", "es",
    {
        "ESONELASUNA",
        "DOSITRESORE",
        "CUATROCINCO",
        "SEISASIETEN",
        "OCHONUEVEYO",
        "LADIEZSONCE",
        "DOCELYMENOS",
        "OVEINTEDIEZ",
        "VEINTICINCO",
        "MEDIACUARTO"
    },
    WORDS, sizeof(WORDS) / sizeof(WORDS[0]), render
};
