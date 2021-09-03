// Animation Engine Example Skeleton
// This sketch shows a basic method for running multiple instances of multiple animations all on the same strip without using any mallocs or frees on a microcontroller.
// I have found many ways to improve upon this concept, but I attempted to keep this sketch simple so that it may serve as a starting point for others and hopefully isn't too overwhelming for those with at least some coding experience.
// To fully understand HOW this sketch works requires some coding knowledge, but if you just want to use it and add your own animations it shouldn't be too bad.
// The "meat" of this sketch is an array of animation objects. Each animation object has it's own function pointer which points to a function that will actually perform the calculation for the animation and write to the CRGB array.
// Every cycle, the controller will iterate through that array and play all the animations inside of it.
// In order to allow multiple instances of the same animations to play simultaneously without needed to dynamically create new objects during runtime, the variables each animation uses are stored in a seperate array.
// Each animation function takes a single integer as an input parameter.
// This input parameter tells the animation where it's variables are stored as well as where it is located in the animation array.
// I have included example animations written at the end of this sketch. Some of these animations use "Advanced techniques" such as automatically creating and deleting animations
// To add your own animations, write the animation function in the same manner as the examples at the bottom of the sketch and declare a new animation object containing the name of the animation, the corresponding function, the variable names, and how many variables are needed
// I may continue to add more to this if it gains enough interest
// Future improvements I could add include: Accompanying bluetooth Android app, Audio Sampling and FFT for music reactive animations, Support for color palletes, Animation variable memory management improvements, Unique loading techniques for different animations
// Garrick Hogrebe https://www.linkedin.com/in/garrick-hogrebe-321043180/

#include <FastLED.h>
FASTLED_USING_NAMESPACE

#define DATA_PIN    15
#define LED_TYPE    WS2812B
#define COLOR_ORDER GRB
#define NUM_LEDS    100
CRGB leds[NUM_LEDS];

#define MAX_ANIMATIONS 20
#define VARIABLES_PER_ANIMATION 7

#define BRIGHTNESS          100
#define FRAMES_PER_SECOND  120



// Some class definitions. In this sketch, these classes are only really neccesary to convay information to the user
// Here we have an "animation" class and a "animationList" class.
// The animation class contains information such as the name of the animation, the function it performs to actually update leds, and the names of variables
// When creating a new animation, the constructor is set up so that it is automatically added to the animation linked list. This list is useful when the user wants to add a new animation through the serial monitor
class animation;

class animationList{
  public:
    animation* start = NULL;
    int listSize = 0;
     
} mainAnimationList;

class animation{
  public:
    String animationName;
    String variableNames;
    int numInputs;
    void (*playFunction)(int index);
    animation* next;

    animation(String newAnimationName, void (*functionPtr)(int), String newVariableNames, int NumInputs){
      animationName = newAnimationName;
      variableNames = newVariableNames;
      playFunction = functionPtr;
      animation* current = mainAnimationList.start;
      numInputs = NumInputs;
      
      if(mainAnimationList.listSize == 0){
        mainAnimationList.start = this;
      }
      else if(mainAnimationList.listSize == 1){
        current->next = this;
      }
      else{
        for(int i = 0; i < mainAnimationList.listSize - 1; i++){
          current = current->next;
        }
        current->next = this;
      }
      mainAnimationList.listSize += 1;
    }   
};




//Function Pointer Array containing each instance of an animation to Cycle Through
animation *animationArray[MAX_ANIMATIONS];

//Array of variables used by each 
int animationVariables[MAX_ANIMATIONS][VARIABLES_PER_ANIMATION];

void setup() {
  //Startup Delay
  delay(3000);

  //Begin the serial moniter. This sketch will use the serial moniter to interact with our controller
  Serial.begin(9600);

  //Setup strips and brightness
  FastLED.addLeds<LED_TYPE,DATA_PIN,COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);

  //Initialize our animation array
  clearAnimations();
  Serial.println("Type something and press enter to edit controller");
}

void loop() {

  //Check the serial monitor to see if
  serialUpdates();

  //This gives all animations a trail like effect
  fadeToBlackBy(leds, NUM_LEDS, 20);
  
  //uncomment this if you don't want a fade effect
  //FastLED.clear();

  //Run through our array of function pointers to play each animations
  playAnimations();

  //Show updates on the strip
  FastLED.show();
  FastLED.delay(1000/FRAMES_PER_SECOND);
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ USER INTERFACE FUNCTIONS ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//Adds an animation through user input in the serial monitor
void addAnimationSerial(){
  int index = -1;
  int incoming = 0;
  int selectionNumber = 0;
  animation* current = mainAnimationList.start;
  
  //Attempt to find a free slot in our animation array
  index = findFreeAnimationSpot();
  //If no spots available inform user and leave
  if(index == -1){
    Serial.println("Animation Array is full");
    return;
  }

  //Have the user select an animation to add
  Serial.println("Select an animation:");
  for(int i = 0; i < mainAnimationList.listSize; i++){
    Serial.print(i);
    Serial.print(": ");
    Serial.println(current->animationName);
    current = current->next;
  }
  while(1){
    if(Serial.available() > 0){
      incoming = Serial.read();
      if(incoming == 10){
        break;
      }
      selectionNumber = selectionNumber*10 + (incoming - 48);
    }
  }
  
  if(selectionNumber >= mainAnimationList.listSize){
    Serial.println("Invalid Selection");
    return;
  }
  
  current = mainAnimationList.start;
  for(int i = 0; i < selectionNumber; i++){
    current = current->next;
  }
  animationArray[index] = current;
  Serial.print("You have selected: ");
  Serial.println(current->animationName);
  //Clear serial input incase user accidently entered something
  while(Serial.available()){
      Serial.read();
    }

 //Have the user fill in the variables
 Serial.println("Enter Variables");
 Serial.println(current->variableNames);
 for(int i = 0; i < current->numInputs; i++){
  Serial.print("Enter variable number: ");
  Serial.println(i);
  selectionNumber = 0;
  while(1){
    if(Serial.available() > 0){
      incoming = Serial.read();
      if(incoming == 10){
        break;
      }
      selectionNumber = selectionNumber*10 + (incoming - 48);
    }
  }
  Serial.print("Received: ");
  Serial.println(selectionNumber);
  animationVariables[index][i] = selectionNumber;
 }
  
}

void printDebugInfo(){
  Serial.println("~~~~~~~~~~~~~~~~~~~DEBUG INFO~~~~~~~~~~~~~~~~~~~");
  for(int i = 0; i < MAX_ANIMATIONS; i++){
    Serial.print("Index: ");
    Serial.print(i);
    if(animationArray[i] == NULL){
      Serial.println(" EMPTY");
      continue;
    }
    Serial.print(" Name: ");
    Serial.print(animationArray[i]->animationName);
    for(int j = 0; j < animationArray[i]->numInputs; j++){
      Serial.print(" V");
      Serial.print(j);
      Serial.print(": ");
      Serial.print(animationVariables[i][j]);
      Serial.print(" ");
    }
    Serial.println("");
  }
}

//Deletes an animation through user input in the seerial monitor
void deleteAnimationSerial(){
  int incoming = 0;
  int selectionNumber = 0;
  Serial.println("Enter the index of the animation you wish to delete");
  printDebugInfo();
  while(1){
    if(Serial.available() > 0){
      incoming = Serial.read();
      if(incoming == 10){
        break;
      }
      selectionNumber = selectionNumber*10 + (incoming - 48);
    }
  }
  if(selectionNumber < 0 || selectionNumber >= MAX_ANIMATIONS){
    Serial.println("Invalid Selection");
    return;
  }
  deleteAnimation(selectionNumber);
}

void serialUpdates(){
  int incoming;
  //Check if user has made input
  if(Serial.available() > 0){
    //Clear serial input incase user entered multiple characters
    while(Serial.available() > 0){
      Serial.read();
    }

    //Print Options
    Serial.println("Select an Option");
    Serial.println("1: Add an animation");
    Serial.println("2: Delete an animation");
    Serial.println("3: Clear all animations");
    Serial.println("4: Print Debug Info");
    
    while(1){
      if(Serial.available() > 0){
        incoming = Serial.read();
        break;
      }
    }
    
    //Clear the rest incase user entered multiple characters;
    while(Serial.available() > 0){
      Serial.read();
    }
    //Perform user entered action
    switch(incoming){
      case '1':
        addAnimationSerial();
        break;

      case '2':
        deleteAnimationSerial();
        break;
        
      case '3':
        clearAnimations();
        break;

      case '4':
         printDebugInfo();
         break;
        
     default:
        Serial.println("Invalid Selection");
    }
    Serial.println("Type something and press enter to edit controller");
  }
  
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ ACTUAL ENGINE FUNCTIONS ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

//This function will run through our array of function pointers and play all animations which have been loaded.
//The variable "indexNumber" is passed as a parameter so that each instance of an animation knows where to find it's variables
void playAnimations(){
  for(int indexNumber = 0; indexNumber < MAX_ANIMATIONS; indexNumber++){
    if(animationArray[indexNumber] != NULL){
      animationArray[indexNumber]->playFunction(indexNumber);
    }
  }
}

// "Remove" an animation from our animation array
void deleteAnimation(int index){
  animationArray[index] = NULL;
}


//Sets all function pointers to NULL, efectively clearing our controller
void clearAnimations(){
  for(int i = 0; i < MAX_ANIMATIONS; i++){
    animationArray[i] = NULL;
  }
}

//Goes through our animation array and attempts to find a free location. This function is called by animations that will create other animations
int findFreeAnimationSpot(){
  for(int i = 0; i < MAX_ANIMATIONS; i++){
    if(animationArray[i] == NULL){
      return i;
    }
  }
  //No slot found
  return -1;
}



//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ANIMATION FUNCTIONS~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//Below is all of the animations. To add your own follow the format of the example functions and then create a new animation object. The parameters for the constructor are: animation name, corresponding function, variable names, number of variables
//To ensure everything works, all animations should be written in  a non blocking way

// A recreation of the sinelon animation from demo reel 100 made to fit this engine
void sinelon(int index){
  int start = animationVariables[index][0];
  int end = animationVariables[index][1];
  int bpm = animationVariables[index][2];
  int hue = animationVariables[index][3];
  int phase = animationVariables[index][4];

  int pos = beatsin16(bpm, start, end, phase);
  leds[pos] += CHSV(hue, 255, 255);
}
animation Sinelon("Sinelon", sinelon, "Start, End, BPM, HUE, Phase", 5);

// Fills a solid section of the strip in
void solidBlock(int index){
  int start = animationVariables[index][0];
  int end = animationVariables[index][1];
  int hue = animationVariables[index][2];

  for(int i = start; i <= end; i++){
    leds[i] += CHSV(hue, 255, 255);
  }
}
animation SolidBlock("Solid Block", solidBlock, "Start, End, Hue", 3);

// Pulses a block in and out
void pulsingBlock(int index){
  int start = animationVariables[index][0];
  int end = animationVariables[index][1];
  int bpm = animationVariables[index][2];
  int hue = animationVariables[index][3];
  int phase = animationVariables[index][4];

  int bright = beatsin16(bpm, 0, 255, phase);
  for(int i = start; i <= end; i++){
    leds[i] += CHSV(hue, 255, bright);
  }
  
}
animation PulsingBlock("Pulsing Block", pulsingBlock, "Start, End, BPM, HUE, Phase", 5);

//Sometimes you want to update a variable within your animation. This animation shows an example of how that can be done using pointers
void movingDot(int index){
  int start = animationVariables[index][0];
  int end = animationVariables[index][1];
  int hue = animationVariables[index][2];
  int *pos = &animationVariables[index][3]; //We use a pointer here so we can update the value of this variable everytime this function is called.

  leds[*pos] += CHSV(hue, 255, 255); //Make sure to access the content of the pointer and not the pointer itself
  *pos = start + (start + *pos + 1)%(end + 1); //Position will have a new value next time this function is called
}
animation MovingDot("Moving Dot", movingDot, "Start, End, HUE, Starting Position", 4);

//Sometimes you want temporary animations that can be created by other animations. This temporary animation fades away and then deletes iteself once it is gone
void fadingBlock(int index){
  int start = animationVariables[index][0];
  int end = animationVariables[index][1];
  int hue = animationVariables[index][2];
  int *brightness = &animationVariables[index][3]; //Using another pointer here to keep track of the brightness. When this value is 0 we will delete this 
  int fadeRate = animationVariables[index][4];

  if (fadeRate > 255){
    fadeRate = 255;
  }else if(fadeRate < 1){
    fadeRate = 1;
  }
  
  *brightness = (*brightness * (255 - fadeRate))/255;

  if (*brightness <= 0){
    deleteAnimation(index);
    return;
  }

  for(int i = start; i <= end; i++){
    leds[i] += CHSV(hue, 255, *brightness);
  }
}
animation FadingBlock("Temporary Fading Block", fadingBlock, "Start, End, hue, Initial Brightness (0 - 255), fadeRate (0 - 255)", 5);


void fadingBlockGenerator(int index){
  int start = animationVariables[index][0];
  int end = animationVariables[index][1];
  int period = animationVariables[index][2];
  int *prevTime = &animationVariables[index][3];

  if(millis() - (unsigned long long int) *prevTime > period){
    int index = findFreeAnimationSpot();
    if(index == -1){
      return;
    }
    animationArray[index] = &FadingBlock;
    int pos = random(start, end - 5);
    animationVariables[index][0] = pos;
    animationVariables[index][1] = pos + random(0, 5);
    animationVariables[index][2] = random8();
    animationVariables[index][3] = 255;
    animationVariables[index][4] = random(1, 50);
    *prevTime = millis();
  }
}
animation FadingBlockGenerator("Random Fading Block Generator", fadingBlockGenerator, "Start, End, Period(in milliseconds), Internal Variable (Enter 0)", 4);
