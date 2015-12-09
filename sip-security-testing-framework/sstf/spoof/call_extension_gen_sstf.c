/*
Program: To generate a range of extension numbers(users) and create a .csv file readable by SIPp 
Author: Hemanth Srinivasan
Year: 2008

NOTE:       These files have been tailored for SSTF wizard.

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <strings.h>

int main (int argc, char *argv[]) {

  char *num_str;
  int num_len = 0, i; 
  FILE *scenario_fp;
  char *fp_name;
  int range = 0, prefix_len = 0;
  char *range_str;
  char *prefix_str;
  char *max_str;
  int max_value = 0;
  char path[] = "../spoof/";  
  char fname_prefix[] = {"SIP_"};
  char fname_suffix[] = {"_user_spoof.csv\0"};

 /* SSTF keyword "SIP_from_user_spoof" "SIP_to_user_spoof". Generated files will have the same nomeclature with a .csv 
    extension.
 */


  if (argc != 3) {
    printf("Usage: %s <Extension Range> <'from/to'> (eg: 123xxx)\n", argv[0]);
    exit(0);
  }
  num_len = strlen(argv[1]);
  printf("Argv[1] is %s of length %d\n", argv[1], strlen(argv[1]));

  if (argv[1][num_len - 1] != 'x') {
    printf("Usage: %s <Extension Range> (eg: 123xxx)\n", argv[0]);
    exit(0);
   }
  num_str = (char *)calloc(num_len+2, sizeof(char));
 
  /*fp_name = (char *)calloc(strlen(path)+num_len+5, sizeof(char));
   strcpy(fp_name, path);
   strcat(fp_name, argv[1]); 
  strcat(fp_name, ".csv\0");
*/
 
  fp_name = (char *)calloc(strlen(path)+strlen(fname_prefix)+strlen(argv[2])+strlen(fname_suffix), sizeof(char));
  bzero(fp_name, strlen(fp_name));
  strcpy(fp_name, path);
  strcat(fp_name, fname_prefix);
  strcat(fp_name, argv[2]);
  strcat(fp_name, fname_suffix);

  printf("Creating CSV file.... %s\n", fp_name); 

  scenario_fp = fopen(fp_name, "w+");
  //fputs("SEQUENTIAL\n", scenario_fp); //this will be added in the merged csv generated by Strezz.java
  prefix_len = strchr(argv[1], 'x') - argv[1];
  printf("Prefix length: %d\n", prefix_len);
  prefix_str = (char *)calloc(prefix_len+2, sizeof(char));
  strncpy(prefix_str, argv[1], prefix_len);
  printf("Prefix is: %s\n", prefix_str);

  range = strrchr(argv[1], 'x') - strchr(argv[1], 'x') + 1;
  printf("Range:%0*d\n", range, range);

  max_str = (char *)calloc(range, sizeof(char));
  for (i = 0; i < range; i++) {
     strcat(max_str, "9");
  }
  range_str = (char *)calloc(strlen(max_str)+2, sizeof(char));
  max_value = atoi(max_str);
  printf("Max_value: %d\n", max_value);

  for (i = 0; i <= max_value; i++) {
    sprintf(range_str, "%0*d\n", range, i);
    // printf("Range str is :%s\n", range_str);
    strcpy(num_str, prefix_str);
    strcat(num_str, range_str);
    // printf("Num str is :%s\n", num_str);
    fputs(num_str, scenario_fp);
    bzero(range_str, strlen(max_str)+2);
    bzero(num_str, num_len+2);
  }

  fclose(scenario_fp);
  free(fp_name);
} 