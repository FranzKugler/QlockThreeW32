include <BOSL2/std.scad>

size = 250;
wall = 10;
mask_thickness = 3;
clock_thickness = 20;
ld = 1000 / 60;         // LED distance
grid_thickness = 2;

display = [
 ["E","S","K","I","S","T","A","F","Ü","N","F"],
 ["Z","E","H","N","Z","W","A","N","Z","I","G"],
 ["D","R","E","I","V","I","E","R","T","E","L"],
 ["V","O","R","F","U","N","K","N","A","C","H"],
 ["H","A","L","B","A","E","L","F","Ü","N","F"],
 ["E","I","N","S","X","A","M","Z","W","E","I"],
 ["D","R","E","I","A","U","J","V","I","E","R"],
 ["S","E","C","H","S","N","L","A","C","H","T"],
 ["S","I","E","B","E","N","Z","W","Ö","L","F"],
 ["Z","E","H","N","E","U","N","K","U","H","R"]
];

module body(){
    difference(){
        union(){
            //masking plate
            difference() {
                cuboid([size,size,mask_thickness], anchor=BOTTOM); 
                //letters
                for(x=[0:11]) 
                    for(y=[0:9]) {
                        translate([(5-x)*ld, (4.5-y)*ld*11/10 ,-1])
                        linear_extrude(mask_thickness+2)mirror([1,0,0])text(display[y][x], ld*11/10-8, halign="center", valign="center");
                        echo (x,y,display[x][y]);
                }
                //corners
                for(x=[0:1])for(y=[0:1])
                    translate([6*ld*(2*x-1),5.5*ld*11/10*(2*y-1),-1])cylinder(d=4, h=mask_thickness+2, $fn=32);
                    
           }
           
           //frame
           difference(){
            cuboid([size,size,clock_thickness], anchor=BOTTOM);
            translate([0,0,-1])cuboid([size-2*wall,size-2*wall,clock_thickness+2], anchor=BOTTOM);
           }
       }
       translate([0,0,0.4])grid(grid_thickness+0.4);
       translate([0,0,5])grid(grid_thickness+0.4);
    }
}

module grid(gt){
    l=size-2*wall + 6;
    h=clock_thickness-2.6;
    for(x=[0:13])translate([(6.5-x)*ld,0,0])cuboid([gt, l, h], anchor=BOTTOM); 
    for(y=[0:12])translate([0,(6-y)*ld*11/10,0])cuboid([l,gt, h], anchor=BOTTOM); 
}



body();
//color("black")translate([0,0,0.6])grid(grid_thickness);
// translucent Plate
color("lightblue", alpha=0.3)cuboid([size,size,3], anchor=TOP);