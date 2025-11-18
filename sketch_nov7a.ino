#include <GxEPD2_BW.h>

#define CS   5
#define DC   17
#define RST  16
#define BUSY 4

// GxEPD2_BW<GxEPD2_213_B73, GxEPD2_213_B73::HEIGHT> display(GxEPD2_213_B73(CS, DC, RST, BUSY));
GxEPD2_BW<GxEPD2_213_BN, GxEPD2_213_BN::HEIGHT> display(GxEPD2_213_BN(CS, DC, RST, BUSY));

int randNum;

void setup() {
  delay(100);
  display.init(1000000);
  display.setRotation(1);
  display.setTextColor(GxEPD_BLACK);
  display.setFont(NULL);

  // initial frame
  drawFrame();
}

void loop() {
  randNum = random(1, 500);
  drawFrame();

  delay(5000); 
}

void drawFrame() {
  display.setPartialWindow(20, 30, 50, 10); // x, y, width, height
  display.firstPage();
  do {
    display.fillRect(20, 30, 50, 10, GxEPD_WHITE);
    display.setCursor(20, 30);
    display.print(randNum);
  } while (display.nextPage());
}
