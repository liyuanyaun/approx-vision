#! /usr/bin/env python

import subprocess
import sys
from os import listdir
from os.path import isfile, join

# The directory to convert
image_dir   = '/work/mark/datasets/coco-2014/val2014/'

# Number of threads to use
num_threads = 15

# Get list of files in directory
file_list = [f for f in listdir(image_dir) if isfile(join(image_dir, f))]
file_list.sort()

# Number of images in the directory
num_imgs    = len(file_list)

# Make a directory for the newly converted data
subprocess.call(['mkdir',image_dir+'converted'])

# Initialize bounds for running the pipe
imgs_per_thread = num_imgs / num_threads
start_img_id = 0
end_img_id   = start_img_id + imgs_per_thread 

# For every split
for x in range(0,num_threads):
  # Provide a temporary folder for this script
  subprocess.call(['mkdir',image_dir+'temp'+str(x)])
  # In case the number of images is not a multiple of the number of threads
  if (end_img_id > num_imgs):
    end_img_id = num_imgs
  # Start a script to process a subset of the images
  pid = subprocess.Popen(["./run-pipe.py",
                          str(x),
                          str(start_img_id),
                          str(end_img_id),
                          image_dir
                         ])
  # Update the bounds for the next script
  start_img_id = start_img_id + imgs_per_thread
  end_img_id   = end_img_id   + imgs_per_thread
