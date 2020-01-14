#ifndef CAMERA_POOL_HPP
#define CAMERA_POOL_HPP

#include <iostream>
using std::cout;
#include <vector>
using std::vector;
#include <string>
using std::string;
#include <unordered_map>
using std::unordered_map;

#include <opencv2/opencv.hpp>
using namespace cv;


class CameraPool
{
  vector<bool> cameras;
  // vector<VideoCapture> cameras;
  const int MAXCAMS = 20;
  unordered_map<int, string> lastframe;

  public:
  CameraPool()
  {
    //initialize camera pool with all available
    for(int i = 0; i < MAXCAMS; i++)
    {
      try
      {
        auto cam = VideoCapture(i);
        if(cam.isOpened())
        {
	  //try to get frame before assigning video device
          Mat testFrame;
          cam.read(testFrame);
          cameras.push_back(true);
          // cameras.push_back(cam);
          lastframe[i] = "";
        }
      }
      catch(...)
      {}
    }
  }

  string getFrame(int cam)
  {
    string encoded = "";
    // TODO consider using exception
    if(cam <= cameras.size())
    {
      try
      {
        auto camera = VideoCapture(cam);
        //read and encode frame
        Mat frame;
        camera.read(frame);
        // cameras[cam].read(frame);
        std::vector<unsigned char> buffer;
        cv::imencode(".jpg", frame, buffer, std::vector<int>());
        encoded = std::string(buffer.begin(), buffer.end());
        lastframe[cam] = encoded;
      }
      catch(Exception &e)
      {
        cout<<"error reading camera: "<<cam<<"\n";
        cout<<e.what()<<"\n";
        cout<<"attempting restart...\n";
        try
        {
          auto camera = VideoCapture(cam);
          // cameras[cam] = VideoCapture(cam);
          if(camera.isOpened())
          {
            cout<<"restarted\n";
            Mat frame;
            // cameras[cam].read(frame);
            camera.read(frame);
            std::vector<unsigned char> buffer;
            cv::imencode(".jpg", frame, buffer, std::vector<int>());
            encoded = std::string(buffer.begin(), buffer.end());
            lastframe[cam] = encoded;
          }

        }
        catch(...)
        {cout<<"doublefail\n";}
      }
    }

    return lastframe[cam];
  }

  int count()
  {
    return cameras.size();
  }
};

#endif
