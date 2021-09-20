//Work in progress sketch for live beat detection with fastLED
//Uses multi threading and parallel processing
//Core 0 runs an interrupt driven fft
//Core 1 interprets the audio data and writes to the leds
//The beat detection attempts to adjust itself based on the song but still could be improved
//There is a section near the bottom you can uncomment if you want to see whats being analized on the serial plotter
//Code is not particularly cleaned up but sharing anyway - You have been warned

#include <arduinoFFT.h>
#define FASTLED_ALLOW_INTERRUPTS 1
#include <FastLED.h>


#define SAMPLES         1024          
#define SAMPLING_FREQ   40000         
#define NOISE           5000 
#define NUM_BANDS 16
#define DATA_PIN         15             
#define COLOR_ORDER     GRB           
#define LED_TYPE         WS2812B       
#define BRIGHTNESS          96
#define FRAMES_PER_SECOND  240
#define NUM_LEDS 300

CRGB leds[NUM_LEDS];

static hw_timer_t *timer = NULL;
double localSamples[2][SAMPLES];
double localSamplesImaginary[2][SAMPLES];

SemaphoreHandle_t full_buffers;
SemaphoreHandle_t empty_buffers;
SemaphoreHandle_t ready_to_load;
SemaphoreHandle_t ready_to_display;

QueueHandle_t audioDataQueue;

class bandVals{
  public:
  int bandValues[16];
};


int16_t bufPos = 0;
uint8_t sampleBuffer = 0;
//Interrupt for sampling
void IRAM_ATTR sampleInterrupt(){
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  //If current buffer full
  if(bufPos >= SAMPLES){
    //Serial.println(bufPos);
    //Check if other buffer ready (Via semaphore)
    if(xSemaphoreTakeFromISR(empty_buffers, NULL)){//If ready give semaphore to let FFT know, switch to new buffer, counter = 0
      //Serial.println("taken");
      if(!xSemaphoreGiveFromISR(full_buffers, &xHigherPriorityTaskWoken)){
        Serial.println("Couldn't give full buffer");//This should never happen but checking just in case     
      }
      sampleBuffer = (sampleBuffer + 1)%2;
      bufPos = 0;    
    }
    else{//If not ready return
      //Serial.println("all buffers full");
      return;
    }
  }
  //DO THE ACTUAL MEASUREMENT NOW OR YOUR FIRED
  localSamples[sampleBuffer][bufPos] = analogRead(36);
  //Serial.println(localSamples[sampleBuffer][bufPos]);
  localSamplesImaginary[sampleBuffer][bufPos] = 0;
  bufPos++;
  
  //make scheduler do thing
  if (xHigherPriorityTaskWoken){
    portYIELD_FROM_ISR();
  }
}

int FFTCount = 0; //Debugging variable

//FFT Task
void FFTLoop(void * param){
  uint8_t index = 0;//Indicates which data buffer we will be doing the FFT on
  bandVals newBandVals;
  int bandValues[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}; // The length of these arrays must be >= NUM_BANDS
  arduinoFFT FFT[2] = {arduinoFFT(localSamples[0], localSamplesImaginary[0], SAMPLES, SAMPLING_FREQ), arduinoFFT(localSamples[1], localSamplesImaginary[1], SAMPLES, SAMPLING_FREQ)};
  //Set up the ADC timer interrupt on core 0
  timer = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, &sampleInterrupt, true);
  timerAlarmWrite(timer, 25, true);
  timerAlarmEnable(timer);

  while(1){
    //Attempt to take a full buffer
    if(!xSemaphoreTake(full_buffers, 0)){//No buffers are ready to perform fft
      //Serial.println("No Buffers for fft");
      continue;
    }
    //We have succesfully taken the semaphore, Begin FFT
    FFT[index].DCRemoval();
    FFT[index].Windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
    FFT[index].Compute(FFT_FORWARD);
    FFT[index].ComplexToMagnitude();
    for (int i = 0; i<NUM_BANDS; i++){
      newBandVals.bandValues[i] = 0;
    }
    for (int i = 2; i < (SAMPLES/2); i++){       // Don't use sample 0 and only first SAMPLES/2 are usable. Each array element represents a frequency bin and its value the amplitude.
      if (localSamples[index][i] > NOISE) {                    // Add a crude noise filter
  
      //16 bands, 12kHz top band
        if (i<=2 )           newBandVals.bandValues[0]  += (int)localSamples[index][i];
        if (i>2   && i<=3  ) newBandVals.bandValues[1]  += (int)localSamples[index][i];
        if (i>3   && i<=5  ) newBandVals.bandValues[2]  += (int)localSamples[index][i];
        if (i>5   && i<=7  ) newBandVals.bandValues[3]  += (int)localSamples[index][i];
        if (i>7   && i<=9  ) newBandVals.bandValues[4]  += (int)localSamples[index][i];
        if (i>9   && i<=13 ) newBandVals.bandValues[5]  += (int)localSamples[index][i];
        if (i>13  && i<=18 ) newBandVals.bandValues[6]  += (int)localSamples[index][i];
        if (i>18  && i<=25 ) newBandVals.bandValues[7]  += (int)localSamples[index][i];
        if (i>25  && i<=36 ) newBandVals.bandValues[8]  += (int)localSamples[index][i];
        if (i>36  && i<=50 ) newBandVals.bandValues[9]  += (int)localSamples[index][i];
        if (i>50  && i<=69 ) newBandVals.bandValues[10] += (int)localSamples[index][i];
        if (i>69  && i<=97 ) newBandVals.bandValues[11] += (int)localSamples[index][i];
        if (i>97  && i<=135) newBandVals.bandValues[12] += (int)localSamples[index][i];
        if (i>135 && i<=189) newBandVals.bandValues[13] += (int)localSamples[index][i];
        if (i>189 && i<=264) newBandVals.bandValues[14] += (int)localSamples[index][i];
        if (i>264          ) newBandVals.bandValues[15] += (int)localSamples[index][i];
      }
    }
  //Serial.println(bandValues[0]);
  //Done with the FFT, give an empty_buffer
  if(!xSemaphoreGive(empty_buffers)){
    //This should never happen, but i've included it to test
    Serial.println("Could not give empty buff");
  }
  //Update the index so next FFT is on the next buffer
  index = (index + 1)%2;

  //add band values to queue
  if (xQueueSend(audioDataQueue, (void *)&newBandVals, 0) != pdTRUE){
    Serial.println("Queue Full");
  }
  
  FFTCount++;
  }
  

}

int showCount = 0;//Debugging variable

//Display LEDS task
void displayLoop(void * param){

  while(1){
    //Wait until the CRGB array has been loaded
    xSemaphoreTake(ready_to_display, portMAX_DELAY);
    //Display the new leds
    FastLED.show();
    showCount++;
    xSemaphoreGive(ready_to_load);

    //Delay this task until enough time has passed to keep a consistent frame rate
    vTaskDelay((1000/FRAMES_PER_SECOND)/portTICK_RATE_MS);
  }
}

//Beat detection and Writing LEDS task
void writeLoop(void * param){
  int pos = 0;
  float testF = 2.2;
  bool newAudio;
  bandVals bandValues;
  bandVals prevBandValues;
  int flux = 0;
  int realFlux = 0;
  int prevFlux = 0;
  int slowDecayPeak = 0;
  int fastDecayPeak = 0;
  int slowDecayMaxFlux = 0;
  int fastDecayMaxFlux = 0;
  int averagePosFlux = 0;
  int prevBandVal = 0;
  int bandVal = 0;
  int lastPart = 0;
  bool beat = true;
  bool fluxHasDecreased = true;
  bool partOn[10];
  for(int x = 0; x < 10; x++){
    partOn[x] = false;
  }

  while(1){
    beat = false;
    if (xQueueReceive(audioDataQueue, (void *)&bandValues, 0) == pdTRUE){
      newAudio = true;
      //Serial.println("rec");
    }else{
      xSemaphoreTake(ready_to_load, portMAX_DELAY);
      xSemaphoreGive(ready_to_display);
      continue;
    }

    bandVal = 0;
    
    //Trying something new -> combine the last two fourier transforms values
    //bandVal = bandValues.bandValues[0] + prevBandValues.bandValues[0];
    
    prevBandValues.bandValues[0] = bandValues.bandValues[0];

    bandVal = bandValues.bandValues[0];
    
    realFlux = bandVal - prevBandVal;
    if(realFlux > 0){
      flux = realFlux;
      averagePosFlux = (averagePosFlux*5 + 5*flux)/10;
    }else {
      flux = 0;
      averagePosFlux = (95*averagePosFlux)/100;
    }

    if(flux > fastDecayMaxFlux){
      fastDecayMaxFlux = flux;
      if(fluxHasDecreased){
        beat = true;
      }
    }else fastDecayMaxFlux = (fastDecayMaxFlux*8)/10;

    if(flux > slowDecayMaxFlux){
      slowDecayMaxFlux = flux;
    }else {
      if(flux == 0){
        slowDecayMaxFlux = slowDecayMaxFlux*995/1000;
      }else{
        slowDecayMaxFlux = (slowDecayMaxFlux*999 + flux)/1000;
      }    
    }

    if(bandVal > fastDecayPeak){
      fastDecayPeak = bandVal;
    }else fastDecayPeak = (fastDecayPeak*9)/10;

    if(bandVal > slowDecayPeak){
      slowDecayPeak = bandVal;
    }else slowDecayPeak = (slowDecayPeak*999 + bandVal)/1000;

    if(fastDecayMaxFlux < slowDecayMaxFlux*3/8){
      beat = false;
    }

    if(fastDecayMaxFlux < averagePosFlux || flux == 0){
      beat = false;
    }
    
    if(flux < prevFlux){
      fluxHasDecreased = true;
    }
    prevFlux = flux;
    prevBandVal = bandVal;

    if(flux > 0){
      //beat = true;
    }

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~Uncomment this section to see results in serial monitor~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    /*
    Serial.print(flux);
    Serial.print(",");
    Serial.print(fastDecayMaxFlux);
    Serial.print(",");
    Serial.print(slowDecayMaxFlux*3/8);
    Serial.print(",");
    Serial.println(averagePosFlux);
    */

    
    //Serial.print(slowDecayMaxFlux/2);
    
    //All writing to leds should be done wihtin this critical section
    xSemaphoreTake(ready_to_load, portMAX_DELAY);
    fadeToBlackBy(leds, NUM_LEDS, 255);
    
    if(beat){
      fluxHasDecreased = false;
      int ran = random(0, 10);
      if(lastPart == ran){
        ran = (ran + 1)%10;
      }
      lastPart = ran;
      if(partOn[ran]){
        partOn[ran] = false;
      }else partOn[ran] = true;
    }
    for(int x = 0; x < 10; x++){
      if(partOn[x]){
        for(int i = 0; i < 10; i++){
          leds[x*10 + i] = CHSV(255,255,255);
        }
      }
    }
    
    xSemaphoreGive(ready_to_display);
  }
}

void setup() {
  // put your setup code here, to run once:
  delay(1000);
  FastLED.addLeds<LED_TYPE,DATA_PIN,COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);
  Serial.begin(115200);
  full_buffers = xSemaphoreCreateCounting(1, 0);
  empty_buffers = xSemaphoreCreateCounting(1, 1);
  ready_to_display = xSemaphoreCreateCounting(1, 0);
  ready_to_load = xSemaphoreCreateCounting(1, 1);
  audioDataQueue = xQueueCreate(1, sizeof(bandVals));
  disableCore0WDT();
  xTaskCreatePinnedToCore(FFTLoop, "FFT Task", 10000, NULL, configMAX_PRIORITIES - 1, NULL, 0);
  xTaskCreatePinnedToCore(displayLoop, "display Task", 10000, NULL, configMAX_PRIORITIES - 1 , NULL, 1);
  xTaskCreatePinnedToCore(writeLoop, "write Task", 10000, NULL, 5, NULL, 1);
}

void loop() {//We don't use the loop, everything on its own task
  vTaskDelay(10000000/portTICK_RATE_MS);
  /*
  Serial.print("NUM FFT: ");
  Serial.println(FFTCount);
  Serial.print("NUM WRITE: ");
  Serial.println(showCount);
  */
}
