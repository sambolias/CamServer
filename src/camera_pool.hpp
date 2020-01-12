#ifndef CAMERA_POOL_HPP
#define CAMERA_POOL_HPP

#include <vector>
using std::vector;
#include <string>
using std::string;

#include <opencv2/opencv.hpp>
using namespace cv;


class CameraPool
{
  vector<VideoCapture> cameras;
  const int MAXCAMS = 20;

  public:
  CameraPool()
  {
    //initialize camera pool with all available
    for(int i = 0; i < MAXCAMS; i++)
    {
      auto cam = VideoCapture(i);
      if(cam.isOpened())
      {
        cameras.push_back(cam);
      }
      else break;
    }
  }

  string getFrame(int cam)
  {
    string encoded = "";
    // TODO consider using exception
    if(cam <= camera.size)
    {
      //read and encode frame
      Mat frame;
      camera.read(frame);
      std::vector<unsigned char> buffer;
      cv::imencode(".jpg", frame, buffer, std::vector<int>());
      encoded = std::string(buffer.begin(), buffer.end());
    }

    return encoded;
  }
};

#endif
