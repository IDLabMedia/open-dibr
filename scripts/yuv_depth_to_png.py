############################################################
# This script converts a frame from a depth map            #
#(i.e. the Y channel contains the depth values) with       #
# pixel format yuv420p/yuv420p10le/yuv420p12le/yuv420p16le #
# to a PNG file with pixel format gray/gray16be            #
############################################################

import sys
import argparse
import os.path
import numpy as np
import cv2

# read in depth map
def read_depth_yuv(path, framenr, w, h, pix_fmt):
    if pix_fmt == "yuv420p":
        nr_y_bytes_per_frame = w * h
        np_dtype = np.uint8
    else:
        nr_y_bytes_per_frame = w * h * 2
        np_dtype = np.uint16
    nr_bytes_per_frame = int(nr_y_bytes_per_frame * 3 / 2)
    try:
        with open(path, 'rb') as file:
            # seek the correct framenr
            file.seek(framenr * nr_bytes_per_frame)
            raw = file.read(nr_y_bytes_per_frame)
    except Exception as e:
        print("Something went wrong while reading the input file", path)
        raise e
        
    # only read the y channel
    y_pixels = np.frombuffer(raw, dtype=np_dtype)
    y_pixels = y_pixels.reshape((h,w))
    print("Succesfully read in", path)
    return y_pixels

# save as 16 bit PNG
def write_to_png(y_pixels, path):
    try:
        cv2.imwrite(path, y_pixels)
    except Exception as e:
        print("Something went wrong while writing to the output file", path)
        raise e
    print("Succesfully wrote to", path)
    
if __name__ == "__main__":
    # parse command line args
    parser = argparse.ArgumentParser(description="This script converts a frame from a depth map (i.e. the Y channel contains the depth values) with pixel format yuv420p/yuv420p10le/yuv420p12le/yuv420p16le to a PNG file with pixel format gray or gray16be.", epilog="EXAMPLE: python yuv_depth_to_png.py in.yuv 1920 1080 25 yuv420p16le out.png")
    parser.add_argument('input_file', type=str,help='the path to input file')
    parser.add_argument('width', type=int,help='the width of the input yuv file in number of pixels')
    parser.add_argument('height', type=int,help='the height of the input yuv file in number of pixels')
    parser.add_argument('frame_nr', type=int,help='the framenumber that needs to be extraced (starting from 0)')
    parser.add_argument('pix_fmt', type=str,help='the pixel format of the input yuv file, should be one of [yuv420p, yuv420p10le, yuv420p12le, yuv420p16le]')
    parser.add_argument('output_file', type=str,help='the path to output file')
    # optional
    parser.add_argument('-v', '--visualize', action="store_true", help='if specified, opens a window showing the resulting image')
    
    args = vars(parser.parse_args())
    in_path = args['input_file']
    w = args['width']
    h = args['height']
    framenr = args['frame_nr']
    pix_fmt = args['pix_fmt']
    out_path = args['output_file']
    visualize = args['visualize']
    
    # some checks of the command line args
    accepted_pix_fmts = ["yuv420p", "yuv420p10le", "yuv420p12le", "yuv420p16le"]
    if pix_fmt not in accepted_pix_fmts:
        print("Error: pix_fmt", pix_fmt, "should be one of", accepted_pix_fmts, ". You can check the pixel format through ffprobe.")
        sys.exit()
    
    if os.path.exists(out_path):
        # if output file already exist, ask if it should be overwritten
        should_overwrite = input("Output file already exists, are you sure you want to overwrite it? [y/n]")  
        if not(should_overwrite == "y" or should_overwrite == "Y"): 
            sys.exit() 
    
    # read input file
    y_pixels = read_depth_yuv(in_path, framenr, w, h, pix_fmt)
    
    if pix_fmt == "yuv420p10le":
        # need to scale up 10-bit values to 16-bit per pixel
        y_pixels = y_pixels * 64
    elif pix_fmt == "yuv420p12le":
        # need to scale up 12-bit values to 16-bit per pixel
        y_pixels = y_pixels * 16
    
    # write output file
    write_to_png(y_pixels, out_path)

    if visualize:
        cv2.imshow('y_pixels',y_pixels)
        print("Press any key (while the new window is selected) to close the window")
        cv2.waitKey(0) 
        cv2.destroyAllWindows()
