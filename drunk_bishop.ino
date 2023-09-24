/*
 * @brief the purpose of this program is to vary the color displayed on a LED light strip
 * in a unique way.
 * 
 * The algorithm is based off of the OpenSSH RandomArt fingerprinting algorithm: Drunk Bishop,
 * which I think is completely appropriate for a bar LED setup. The algorithm is described below:
 * 
 * 1. Initialize a 3D cube corresponding to the Red, Green, and Blue dimensions. The range of each
 * dimension are discrete values from 0 to 255, inclusive. Each grid coordinate therefore corresponds
 * to a unique color
 * 2. The input values of the algorithm are the step size, starting point, and string. The step size
 * controls the sizez of the 3D cube (step size of 1 creates 256x256x256, step size of 8 creates 32x32x32 cube).
 * The starting point determines what the initial color will be. The string determines the walking path.
 * To ensure that the color paths are unique, the starting point and walking path should be random. The starting
 * path should be easy to generate using a uniform random distribution, but I think it would be interesting to
 * use a 128 or 256 bit hash of some quantity like the glass weight. You could theoretically even use the first 3*log2(256/step_size)
 * bits of the hash to initialize the starting point and the remaining bits as the path so that the same glass will always
 * have the same light show. 
 * 3. Starting at the starting point, proceed by parsing every 3 bits of the path. The 3 bits will determine the next step direction:
 * NW, NE, SW, SE, FN, FS, RN, RS where F denotes forward, R denotes reverse. The path should ideally not be divisible by 3 so that the
 * steps don't repeat until the (length + 1) iteration rather than the (length/2 + 1) iteration. 
 * 4. Output is the new RGB value corresponding to the coordinate 
 * 
 * Design in two parts:
 * -  init function: returns the struct that will be used to keep track of the system's state
 * -  step function: updates the struct's state by progressing its path one step forward
 */

#include <HX711.h>
#include <XxHash_arduino.h>

//global constants
#define ERROR   -1;
#define SUCCESS  0;

typedef struct rgb_cube
{
  int16_t location[3];    //current position of the algorithm in format: R, G, B
  int     step_size;      //discrete RGB bin size that determines how big the cube is. Step size 8 creates a 32x32x32 cube
  int     path_idx;       //keeps track of how far along the path the algorithm is
  int     path_len;       //length of the path, in bits
  char *  path;           //the algorithm's path
} rgb_cube_t;

//global variables
rgb_cube_t * bar_cube;
int path_size = 17;
int display_delay = 30;
int display_increment = 5;
int step_size = 16; //discrete RGB bin size that determines how big the cube is. Step size 8 creates a 32x32x32 cube (256 / 8 = 32). Step size 1 is maximum cube size.
char path[17] = "this is a test!!"; //actually 16 characters, creating one extra for the null byte

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

void setup()
{
  Serial.begin(9600);

   //initialization parameters, tune these
  int16_t start_point[3] = {255, 255, 255}; //more or less the middle of the cube

  //hash path
  char hashed_path_string[34] = { 0 };
  char hashed_path_char[17] = { 0 };
  int index = 0;
  for(int i = 0; i < 4; i++)
  {
    char hash[9] = { 0 };
    char shortened_string[4] = { 0 };
    memcpy(shortened_string, path[i*4], 4);
    xxh32(hash, shortened_string);
    strcat(hashed_path_string, hash);
    char result;
    for(int j = 0; j < 4; j++)
    {
      char hex_input[3] = { 0 };
      strncpy(hex_input, hash + (j*2), 2);
      long val = strtol(hex_input, NULL, 16);
      hashed_path_char[index] = (char) val;
      index = index + 1;
    }
  }
  Serial.println(hashed_path_string);
  Serial.println(hashed_path_char);

  //create the cube - unhashed version
  //bar_cube = rgb_cube_init(step_size, path, path_size, start_point);

  //create the cube - hashed version
  bar_cube = rgb_cube_init(step_size, hashed_path_char, path_size, start_point);
}

void loop()
{
  int16_t prev_location[3] = {bar_cube->location[0], bar_cube->location[1], bar_cube->location[2]};
  
  //increment one step in time
  rgb_cube_step(bar_cube);

  //debug print
  rgb_cube_display(bar_cube, true);

  //display smooth colors
  int dirs[3] = {(bar_cube->location[0] - prev_location[0])/step_size, (bar_cube->location[1] - prev_location[1])/step_size, (bar_cube->location[2] - prev_location[2])/step_size};
  for(int i = 0; i < step_size; i += display_increment)
  {
    analogWrite(9, prev_location[0] + i * dirs[0]);  //R led
    analogWrite(10, prev_location[1] + i * dirs[1]); //G led
    analogWrite(11, prev_location[2] + i * dirs[2]); //B led

    delay(display_delay);
  }
}
