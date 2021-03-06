
#include "Halide.h"
#include "halide_image_io.h"
#include <opencv2/opencv.hpp>
#include <stdio.h>
#include <math.h>
#include "../common/pipe_stages.h"
#include "../common/ImgPipeConfig.h"
#include "../common/LoadCamModel.h"
#include "../common/MatrixOps.h"

// Pipeline V19 
// 
// Test type: 
// Raw data w/ linear sampling: 2 bits
// 
// Stages:
// V1, Requant2
//
// Note: V1 must be run first. This expects input binary to be result of V1

int main(int argc, char **argv) {

  int num_bits = 2;

  using namespace std;  

  // Inform user of usage method
  if ( argc != 3 ) 
  {
      printf("usage: \n./convert in/data/name.bin out/data/name.bin \n");
      return -1; 
  }

  // Input data directory
  const char * in_data_path   = argv[1];
  // Output data directory
  const char * out_data_path  = argv[2];

  ///////////////////////////////////////////////////////////////////////////////////////
  // Import and format model data

  // Declare model parameters
  vector<vector<float>> Ts, Tw, TsTw;
  vector<vector<float>> rev_ctrl_pts, rev_weights, rev_coefs;
  vector<vector<float>> ctrl_pts, weights, coefs;
  vector<vector<float>> rev_tone;

  // Load model parameters from file
  // NOTE: Ts, Tw, and TsTw read only forward data
  // ctrl_pts, weights, and coefs are either forward or backward
  // tone mapping is always backward
  // This is due to the the camera model format
  Ts            = get_Ts       (cam_model_path);
  Tw            = get_Tw       (cam_model_path, wb_index);
  TsTw          = get_TsTw     (cam_model_path, wb_index);
  rev_ctrl_pts  = get_ctrl_pts (cam_model_path, num_ctrl_pts, 0);
  rev_weights   = get_weights  (cam_model_path, num_ctrl_pts, 0);
  rev_coefs     = get_coefs    (cam_model_path, num_ctrl_pts, 0);
  ctrl_pts      = get_ctrl_pts (cam_model_path, num_ctrl_pts, 1);
  weights       = get_weights  (cam_model_path, num_ctrl_pts, 1);
  coefs         = get_coefs    (cam_model_path, num_ctrl_pts, 1); 
  rev_tone      = get_rev_tone (cam_model_path);

  // Take the transpose of the color map and white balance transform for later use
  vector<vector<float>> TsTw_tran     = transpose_mat(TsTw);

  // Create an inverse of TsTw_tran
  vector<vector<float>> TsTw_tran_inv = inv_3x3mat(TsTw_tran);

  using namespace Halide;
  using namespace Halide::Tools;
  using namespace cv;

  // Convert backward control points to a Halide image
  int width  = rev_ctrl_pts[0].size();
  int length = rev_ctrl_pts.size();
  Image<float> rev_ctrl_pts_h(width,length);
  for (int y=0; y<length; y++) {
    for (int x=0; x<width; x++) {
      rev_ctrl_pts_h(x,y) = rev_ctrl_pts[y][x];
    }
  }
  // Convert backward weights to a Halide image
  width  = rev_weights[0].size();
  length = rev_weights.size();
  Image<float> rev_weights_h(width,length);
  for (int y=0; y<length; y++) {
    for (int x=0; x<width; x++) {
      rev_weights_h(x,y) = rev_weights[y][x];
    }
  }

  // Convert forward control points to a Halide image
  width  = ctrl_pts[0].size();
  length = ctrl_pts.size();
  Image<float> ctrl_pts_h(width,length);
  for (int y=0; y<length; y++) {
    for (int x=0; x<width; x++) {
      ctrl_pts_h(x,y) = ctrl_pts[y][x];
    }
  }
  // Convert forward weights to a Halide image
  width  = weights[0].size();
  length = weights.size();
  Image<float> weights_h(width,length);
  for (int y=0; y<length; y++) {
    for (int x=0; x<width; x++) {
      weights_h(x,y) = weights[y][x];
    }
  }

  // Convert the reverse tone mapping function to a Halide image
  width  = 3;
  length = 256;
  Image<float> rev_tone_h(width,length);
  for (int y=0; y<length; y++) {
    for (int x=0; x<width; x++) {
      rev_tone_h(x,y) = rev_tone[y][x];
    }
  }
 
  ///////////////////////////////////////////////////////////////////////////////////////
  // Establish IO
  char val, label;
  fstream infile(in_data_path);
  //fstream infile("/work/mark/datasets/cifar-10/cifar-10-batches-bin/test_batch.bin");
  if(!infile) {
    printf("Input file not found: %s\n", in_data_path);
    return 1;
  }  

  fstream outfile;
  outfile.open(out_data_path,fstream::out);

  // Declare image handle variables
  Var x, y, c;

  // Define input image 
  Image<uint8_t> input(32,32,3);

  ///////////////////////////////////////////////////////////////////////////////////////
  // Process Images
 
  for (int i=0; i<10000; i++) { //i<10000

    // Read in label
    infile.read(&val,1);
    label = val;
    //printf("label: %u\n",(unsigned char)label);
    
    for (int c=0; c<3; c++) { 
      for (int y=0; y<32; y++) {
        for (int x=0; x<32; x++) {
          infile.read(&val,1);
          input(x,y,c) = (unsigned char)val;
        }
      }
    }

    //save_image(input, "input.png");

    ///////////////////////////////////////////////////////////////////////////////////////
    // Camera pipeline

    int width  = input.width();
    int height = input.height();

    // No scaling here since performing requantization in 8 bit space
    
    // Requantize
    Func requant = make_requant( &input, num_bits );

   
    ///////////////////////////////////////////////////////////////////////////////////////
    // Scheduling

    // Use JIT compiler
    requant.compile_jit();
    Image<uint8_t> output = requant.realize(width,height,3);    

    ///////////////////////////////////////////////////////////////////////////////////////
    // Save the output

    //save_image(output, "output.png");

    // Read in label
    val = label;
    outfile.write(&val,1);
    //infile.read(&val,1);
    //label = val;
    
    for (int c=0; c<3; c++) { 
      for (int y=0; y<32; y++) {
        for (int x=0; x<32; x++) {
          val = output(x,y,c);
          outfile.write(&val,1);
        }
      }
    }



  }

  return 0;
}


