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
#include <queue>
using std::queue;
#include <memory>
using std::shared_ptr;

#include <opencv2/opencv.hpp>
using namespace cv;


class CameraPool
{
  struct Camera
  {
    shared_ptr<VideoCapture> cam;
    int camId;
    Camera(int cid)
    {
      camId = cid;
      cam = std::make_shared<VideoCapture>(camId);
    }

  };
  vector<int> cameras;
  // todo make lookup table
  queue<Camera> openCams;
  // vector<VideoCapture> cameras;
  const int MAXCAMS = 20;

  //TODO use this instead of hardcoded
  const int MAXOPEN = 2;  // doesn't seem to be enough mem on rpi to hold 3 simultaneously
  unordered_map<int, string> lastframe;


  shared_ptr<VideoCapture> getCam(int cid)
  {
    //TODO this logic only works with MAXOPEN=2
    if(openCams.front().camId == cid)
      return openCams.front().cam;
    if(openCams.back().camId == cid)
      return openCams.back().cam;

    Camera camera(cid);
    if(openCams.size() >= MAXOPEN)
      openCams.pop();
    openCams.emplace(camera);
    return camera;
  }

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
          cameras.push_back(i);
          // cameras.push_back(cam);
          std::vector<unsigned char> buffer;
          cv::imencode(".jpg", testFrame, buffer, std::vector<int>());
          string encoded = std::string(buffer.begin(), buffer.end());
          lastframe[cam] = encoded;
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
        Camera camera = getCam(cameras[cam]);
        //read and encode frame
        Mat frame;
        camera.cam->read(frame);
        // cameras[cam].read(frame);
        std::vector<unsigned char> buffer;
        cv::imencode(".jpg", frame, buffer, std::vector<int>());
        encoded = std::string(buffer.begin(), buffer.end());
        lastframe[cam] = encoded;
      }
      catch(Exception &e)
      {
        cout<<"error reading camera: "<<cameras[cam]<<"\n";
        cout<<e.what()<<"\n";
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
