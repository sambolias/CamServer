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
      try
      {
        auto cam = VideoCapture(i);
        if(cam.isOpened())
        {
	  //try to get frame before assigning video device
          Mat testFrame;
	  cam.read(testFrame);
          cameras.push_back(cam);
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
      //read and encode frame
      Mat frame;
      cameras[cam].read(frame);
      std::vector<unsigned char> buffer;
      cv::imencode(".jpg", frame, buffer, std::vector<int>());
      encoded = std::string(buffer.begin(), buffer.end());
    }

    return encoded;
  }

  int count()
  {
    return cameras.size();
  }
};

#endif
