#include <stdio.h>
#include <stdint.h>
#include "../../gl/include/window.h"


int main(){
    CenoGL::Window *window = new CenoGL::Window(480, 480, "CenoGL");
    window->run();
    return 0;
}