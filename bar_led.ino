#include <HX711.h>
#include <XxHash_arduino.h>

//global constants
#define calibration_factor -10000.0 //This value is obtained using the SparkFun_HX711_Calibration sketch
#define DOUT  3
#define CLK  2
#define ERROR   -1
#define SUCCESS  0
#define WEIGHT_THRESHOLD 0.2
#define HASH_SIZE 9

typedef struct rgb_cube
{
  int16_t location[3];    //current position of the algorithm in format: R, G, B
  int     step_size;      //discrete RGB bin size that determines how big the cube is. Step size 8 creates a 32x32x32 cube
  int     path_idx;       //keeps track of how far along the path the algorithm is
  int     path_len;       //length of the path, in bits
  char *  path;           //the algorithm's path
} rgb_cube_t;

//global variables
HX711 scale;
rgb_cube_t * bar_cube;
int path_size = 19;
int display_delay = 30;
int display_increment = 5;
int step_size = 21;
float base_weight_reading = 0.0;
int white_light_pin = 6;
//initialization parameters, tune these
int16_t start_point[3] = {255, 255, 255}; //more or less the middle of the cube
bool weight_increasing = false;
bool display_light = false;
float white_light = 1.0;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);

  scale.begin(DOUT, CLK);
  scale.set_scale(calibration_factor); //This value is obtained by using the SparkFun_HX711_Calibration sketch
  scale.tare(); //Assuming there is no weight on the scale at start up, reset the scale to 0

  pinMode(9, OUTPUT);
  pinMode(10, OUTPUT);
  pinMode(11, OUTPUT);
  pinMode(white_light_pin, OUTPUT);

  analogWrite(9, 0);
  analogWrite(10, 0);
  analogWrite(11, 0);
  analogWrite(white_light_pin, 0);

  float base_weight_reading = (-1 * scale.get_units());
}

void loop() {
  // put your main code here, to run repeatedly:
  float weight_reading = (-1 * scale.get_units());
  //Serial.print("Weight reading is: ");
  //Serial.println(weight_reading);
  float initial_weight_difference = weight_reading - base_weight_reading;
  if(weight_reading > (base_weight_reading + WEIGHT_THRESHOLD))
  {
    //create the cube based on the weight
    char hash[HASH_SIZE] = { 0 };
    char float_as_str[4] = { 0 };
    memcpy((void *) float_as_str, (const void *) &initial_weight_difference, 4);
    xxh32(hash, float_as_str);
    bar_cube = rgb_cube_init(step_size, hash, HASH_SIZE, start_point);
    //Serial.print("Created bar cube with hash: ");
    //Serial.println(hash);
    display_light = true;
  }
  while(display_light)
  {
    //check to see if the weight is increasing (i.e. the glass is filling up)
    float weight_difference = weight_reading - base_weight_reading;
    if(weight_increasing || (weight_difference > (initial_weight_difference + WEIGHT_THRESHOLD)))
    {
       weight_increasing = true;
    }
    
    int16_t prev_location[3] = {bar_cube->location[0], bar_cube->location[1], bar_cube->location[2]};
    //increment one step in time
    rgb_cube_step(bar_cube);
  
    //debug print
    //rgb_cube_display(bar_cube, true);
  
    //display smooth colors
    int dirs[3] = {(bar_cube->location[0] - prev_location[0])/step_size, (bar_cube->location[1] - prev_location[1])/step_size, (bar_cube->location[2] - prev_location[2])/step_size};
    for(int i = 0; i < step_size; i += display_increment)
    {
      analogWrite(9, prev_location[0] + i * dirs[0]);  //R led
      analogWrite(10, prev_location[1] + i * dirs[1]); //G led
      analogWrite(11, prev_location[2] + i * dirs[2]); //B led
  
      delay(display_delay);
    }
    //if the weight is increasing, pulse the white light
    if(weight_increasing)
    {
      //Serial.println("White light blinking!");
      white_light *= 1.03;
      if(white_light > 255)
      {
        white_light = 1.0;
      }
      analogWrite(white_light_pin, int(white_light));
    }

    //check to see if the glass was removed, if so blink 3 times and reset variables
    weight_reading = (-1 * scale.get_units());
    if(weight_reading < (initial_weight_difference + base_weight_reading - WEIGHT_THRESHOLD))
    {
      analogWrite(9, 0);  //R led
      analogWrite(10, 0); //G led
      analogWrite(11, 0); //B led
      analogWrite(white_light_pin, 0);
      
      int counter = 0;
      white_light = 1;
      while(counter < 5)
      {
        white_light *= 1.2;
        if(white_light > 255)
        {
          white_light = 1;
          counter++;
        }
        analogWrite(white_light_pin, int(white_light));
      }

      analogWrite(white_light_pin, 0);
      weight_increasing = false;
      display_light = false;
      white_light = 1;
      //Serial.println("Glass removed, enjoy your drink!");
    }
  }
  delay(1000);
}

/*
 * @brief print out the values of the rgb_cube so that the user is aware of its state
 * 
 * @param rgb_cube : rgb_cube_t struct containing the state of the cube you wish to display
 */
void rgb_cube_display(rgb_cube_t * rgb_cube, bool locations_only)
{
  //check inputs
  if(NULL == rgb_cube)
  {
    fprintf(stderr, "rgb_cube is NULL\n");
    return;
  }

  //display values
  if(!locations_only)
  {
    int buf_size = 50;
    char str_buf[50];
    memset(str_buf, '\0', buf_size);
  
    if(NULL == rgb_cube->location)
    {
      Serial.println("Location: NULL");
    }
    else
    {
      sprintf(str_buf, "Location:    %d, %d, %d", rgb_cube->location[0], rgb_cube->location[1], rgb_cube->location[2]);
      Serial.println(str_buf);
    }
    
    memset(str_buf, '\0', buf_size);
    sprintf(str_buf, "Step Size:   %u", rgb_cube->step_size);
    Serial.println(str_buf);
  
    memset(str_buf, '\0', buf_size);
    sprintf(str_buf, "Path Index:  %u", rgb_cube->path_idx);
    Serial.println(str_buf);
  
    memset(str_buf, '\0', buf_size);
    sprintf(str_buf, "Path Length: %d", rgb_cube->path_len);
    Serial.println(str_buf);
  
    memset(str_buf, '\0', buf_size);
    sprintf(str_buf, "Path:        %s", rgb_cube->path);
    Serial.println(str_buf);
  
    Serial.println("");
  }
  else
  {
    char str_buf[50];
    memset(str_buf, '\0', 50);
    sprintf(str_buf, "%d %d %d %d %d", bar_cube->location[2], bar_cube->location[0], bar_cube->location[1], 0, 256); //serial plotter displays color blue first, then red then green.
    Serial.println(str_buf);
  }

  return;
}

/*
 * @brief initialize the rgb cube struct
 * 
 * @param step_size     : discrete RGB bin size that determines how big the cube is. Step size 8 creates a 32x32x32 cube
 * @param path          : the algorithm's path signature. should be preallocated. do not free until this struct is freed.
 * @param path_size     : length of the path
 * @param start_point   : array of length 3 that contains the R, G, and B index of the starting point (in that order)
 * 
 * @return rgb_cube_t * : allocated memory for the rgb_cube state
 */
rgb_cube_t * rgb_cube_init(int step_size, char * path, int path_size, int16_t * start_point)
{
  //check input values
  if(NULL == path)
  {
    fprintf(stderr, "path string is null\n");
    return NULL;
  }
  if(NULL == start_point)
  {
    fprintf(stderr, "start point is null\n");
    return NULL;
  }

  //allocate meomry for the RGB cube and initialize its state
  rgb_cube_t * rgb_cube = (rgb_cube_t *) calloc(1, sizeof(rgb_cube_t));

  memcpy((void *) rgb_cube->location, (void *) start_point, 3 * sizeof(int16_t));
  rgb_cube->step_size = step_size;
  rgb_cube->path_idx  = 0;
  rgb_cube->path_len  = 8 * path_size; //the path length, in bits
  rgb_cube->path      = path;

  //return the initial state of the rgb cube
  return rgb_cube;
}

/*
 * 
 * 
 */
int rgb_cube_step(rgb_cube_t * rgb_cube)
{
  //check inputs
  if(NULL == rgb_cube)
  {
    fprintf(stderr, "rgb cube is null\n");
    return ERROR;
  }

  //change the location of the RGB cube based on the next 3 bits in the path
  for(int i = 0; i < 3; i++)
  {
    //get the path_idx bit from the path character array
    int     char_idx     = (rgb_cube->path_idx - (rgb_cube->path_idx % 8)) / 8;
    uint8_t shift_val    = 8 - (rgb_cube->path_idx - (char_idx * 8)) - 1;
    uint8_t shifted_char = rgb_cube->path[char_idx] >> shift_val;
    uint8_t masked_char  = shifted_char & 1;
    int8_t  step_dir     = rgb_cube->step_size;

    //increment the location based on the bit value
    if(0 == masked_char)
    {
      step_dir *= -1;
    }
    rgb_cube->location[i] += step_dir;

    //check boundaries are satisfied
    if(rgb_cube->location[i] < 0 || rgb_cube->location[i] > 255)
    {
      rgb_cube->location[i] += step_dir * -1;
    }

    //increment path index
    rgb_cube->path_idx++;
    if(rgb_cube->path_idx >= rgb_cube->path_len)
    {
      rgb_cube->path_idx = 0;
    }
  }
  return SUCCESS;
}
