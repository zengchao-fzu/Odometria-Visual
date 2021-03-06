#include <stdio.h>
#include <iostream>
#include "opencv2/core.hpp"
#include "opencv2/features2d.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/calib3d.hpp"
#include "opencv2/xfeatures2d.hpp"
#include "opencv2/video/tracking.hpp"
#include "opencv2/core/types.hpp"
#include <opencv2/plot.hpp>

using namespace cv;
using namespace cv::xfeatures2d;
using namespace std;
namespace patch
{
    template < typename T > std::string to_string( const T& n ) // funcion para transformar entero en string
    {
        std::ostringstream stm ;
        stm << n ;
        if (n < 100){
            return "0000"+stm.str();
        }
        if (n >= 100 & n<1000){
            return "000"+stm.str();
        }
            
        return "00"+stm.str() ;
    }
}
  
static void help();
void readme();
int Odometry(Mat img_1, Mat img_2, Mat &R, Mat &t, Mat &imageOut);
void thresholdTrans(vector<KeyPoint> &points, int thresholdy, int w, int h, vector<KeyPoint>  &pointsOut); //verifica si el punto se encuentra en una ventana dada
void thresholdRot(vector<KeyPoint> &points, int thresholdx, int w, int h, vector<KeyPoint> &pointsOut); //verifica si el punto se encuentra en una ventana dada
void checkStatus(vector<Point2f> &points, vector<uchar> &status, vector<Point2f> &pointsOut);



/** @function main */
int main( int argc, char** argv )
{
cv::CommandLineParser parser(argc, argv,
        "{help h||}{@image|../data/pic1.png|}"
    );
    if (parser.has("help"))
    {
        help();
        return 0;
    }


    clock_t begin = clock(); // Tiempo de inicio del codigo

    string file_directory = parser.get<string>("@image"); // Direccion del directorio con imagenes
    int index1_int;
    string index1_str, index2_str;
    Mat src, src2;
    
    index1_int = 57 ; // Primera imagen a leer
    int break_prcs =0;

    Mat odometry = Mat::zeros(1, 3, CV_64F); // Matriz vacia de vectores de longitud 3 (comienza con 000)
    Mat R, t; // matriz de rotacion y traslacion
    Mat R_p = Mat::eye(Size(3, 3), CV_64F); // matriz de rotacion temporal
    Mat traslation = Mat::zeros(3, 1, CV_64F);
    Mat plot_x;
    Mat plot_y;
    Mat img_fast;
   

    while(index1_int < 1049){ // penultima imagen  a leer
        index1_str = file_directory + patch::to_string(index1_int)+".png";
        index2_str = file_directory + patch::to_string(index1_int+1)+".png";
        src = imread(index1_str, 1); // Cargar con color, flag = 1
        src2 = imread(index2_str, 1);

        while ( src2.empty() & index1_int < 1049) // En caso de lectura de una imagen no existe
         {
            index1_int = index1_int+1;// Saltar a la siguiente imagen
            cout<< "Imagen no encontrada:"<< index1_int<< endl;
            index2_str = file_directory + patch::to_string(index1_int+1)+".png";
            src2 = imread(index2_str, 1);
        }
        Mat gray1, gray2;
        cvtColor(src, gray1, CV_BGR2GRAY);
        cvtColor(src2, gray2, CV_BGR2GRAY);
        break_prcs = Odometry(src, src2, R, t, img_fast);
        if (break_prcs == 1)
         break; // Mostrar la imagen hasta que se presione una teclas
        //odometry.push_back(despl)
        //cout<< "Matrix R="<< R<<endl;
        //cout<< "Matrix t="<< t<<endl;
        traslation = traslation +R_p*t;
        R_p = R*R_p;
        odometry.push_back(traslation.t());
        
        plot_x.push_back(traslation.row(0));
        plot_y.push_back(traslation.row(2));
        double min_x, max_x, min_y, max_y;
        minMaxLoc(plot_x, &min_x, &max_x);
        minMaxLoc(plot_y, &min_y, &max_y);
        Mat plot_result;
        Ptr<plot::Plot2d> plot = plot::createPlot2d(plot_x, plot_y);
        plot->setPlotBackgroundColor( Scalar( 0, 0, 0 ) );
        plot->setNeedPlotLine(0);
        plot->setPlotAxisColor (Scalar( 255, 0, 0 ) );
        plot->setMaxY (max_y+10);
        plot->setMinY (min_y-10);
        plot->setMaxX (max_x+10);
        plot->setMinX (min_x-10);
        plot->render( plot_result );

          //imshow("Puntos detectados FAST-1", img_keypoints_1 );
         
        //namedWindow("Puntos detectados FAST-1_bajo analisis",WINDOW_NORMAL);
        //resizeWindow("Puntos detectados FAST-1_bajo analisis", 300, 300);
        Mat img_res1, img_res2;
        resize(img_fast,img_res1, Size( 640,300));//resize image
        resize(plot_result,img_res2, Size(640, 300));//resize image
        Mat img_win;
        vconcat(img_res1, img_res2, img_win);
        imshow( "Fast", img_win);
        
        //cout<< "Traslation" << traslation<< endl;

        //cout<< "Indice =" << index1_int<< endl; 
        index1_int ++; // Siguiente par de imagenes
       
    }

    FileStorage file1("Output_fast.yaml", FileStorage::WRITE); // Archivo para guardar el desplazamiento 
    file1 << "Trayectoria"<< odometry;
    clock_t end = clock();
    double elapsed_secs = double(end - begin) / CLOCKS_PER_SEC;
    cout << "elapsed time = "<< elapsed_secs << endl;
    return 0;
}



static void help()
{
    cout << "\nThis program demonstrates a simple SLAM.\n"
            "Usage:\n"
            "./main.out <directory_name>, Default is ../data/pic1.png\n" << endl;
}
int Odometry(Mat img_1, Mat img_2, Mat &R, Mat &t, Mat &imageOut){
  int w1, h1; // Image size
  w1 = img_1.size().width;
  h1 = img_1.size().height;
  //-- Paso 1: detectar los puntos clave utilizando SIFT
  int minHessian = 400;

  Ptr<KAZE> detector = KAZE::create();
  Ptr<DescriptorMatcher> matcher = DescriptorMatcher::create("BruteForce-Hamming");

  vector<KeyPoint> keypoints_1, keypoints_2; // Vector para almacenar los puntos detectados con FAST
  Mat descriptors_1, descriptors_2;
  detector -> detectAndCompute( img_1, Mat(), keypoints_1, descriptors_1 );
  detector -> detectAndCompute( img_2, Mat(), keypoints_2, descriptors_2 );

  vector< vector<DMatch> > matches; // Cambio
  matcher-> knnMatch( descriptors_1, descriptors_2, matches, 2 );
  double nn_match_ratio = 0.8f; // Nearest-neighbour matching ratio
  vector<KeyPoint> matched1, matched2;
  for(unsigned i = 0; i < matches.size(); i++) {
      if(matches[i][0].distance < nn_match_ratio * matches[i][1].distance) {
          matched1.push_back(keypoints_1[matches[i][0].queryIdx]);
          matched2.push_back(keypoints_2[matches[i][0].trainIdx]);
      }
  }
  //cout << "Puntos detectados = "<< keypoints_1.size()<< endl;
  //-- Paso 2: Definir la region de interes (ROI)
  /*
  int thresholdx = 40, thresholdy = 50;
  thresholdRot(keypoints_1, thresholdx, w1, h1, key_points_ROT_out); // Puntos a considerar para la rotacion
  thresholdTrans(key_points_ROT_out, thresholdy, w1, h1,key_points_TRANS_out); // Puntos a considerar para la traslacion
  */
  //cout << "Puntos bajo analisis ="<< keypointsOK.size()<< endl;



  //-- Paso 4: Calcular la matriz Esencial
    // Parametros intrisecos de la camara
  double fx, fy, focal, cx, cy;
  fx = 7.188560000000e+02;
  fy = 7.188560000000e+02;
  cx =  6.071928000000e+02;
  cy =  1.852157000000e+02;
  focal = fx;
  Mat E; // matriz esencial

  std::vector<Point2f> points1_OK, points2_OK; // Puntos finales bajos analisis
  vector<int> point_indexs;
  cv::KeyPoint::convert(matched1, points1_OK,point_indexs);
  cv::KeyPoint::convert(matched2, points2_OK,point_indexs);

  E = findEssentialMat(points1_OK, points2_OK, focal, Point2d(cx, cy), RANSAC, 0.999, 1.0, noArray());

  //--Paso 5: Calcular la matriz de rotación y traslación de puntos entre imagenes 
   int p;
   p = recoverPose(E, points1_OK, points2_OK, R, t, focal, Point2d(cx, cy), noArray()   );


//-- Paso 6: Draw keypoints
  Mat img_keypoints_1, img_keypoints_2, img_keypointsOK; 

  
  drawKeypoints( img_1, keypoints_1, img_keypointsOK, Scalar::all(-1), DrawMatchesFlags::DEFAULT );

  imageOut = img_keypointsOK;

  char c_input = (char) waitKey(25);
  if( c_input == 'q' | c_input == ((char)27) ) 
    return 1;
  return 0;


}
  /** @function readme */

  void readme()
  { std::cout << " Usage: ./Fast <img1> <img2>" << std::endl; }

  void  thresholdTrans(vector<KeyPoint> & points, int thresholdy, int w, int h, vector<KeyPoint> &points_out){
        KeyPoint point; // Punto final condicionado al umbral
        int px, py; // Puntos temporales
        for (std::vector<KeyPoint>::const_iterator i = points.begin(); i != points.end(); ++i)
        { 
          px = (*i).pt.x;
          py = (*i).pt.y;
          if( (py < (h-thresholdy)) & (py > thresholdy)){
            point.pt.x =  px;
            point.pt.y = py;
            points_out.push_back(point); // Anexar punto umbral al vector de puntos de salida
          }
    
          
        }
    }
  void thresholdRot(vector<KeyPoint>  &points, int thresholdx, int w, int h, vector<KeyPoint>  &points_out)
    {
        KeyPoint point; // Punto final condicionado al umbral
        int px, py; // Puntos temporales
        for (std::vector<KeyPoint>::const_iterator i = points.begin(); i != points.end(); ++i)
        { 
          px = (*i).pt.x;
          py = (*i).pt.y;
          if( (px < (w-thresholdx)) & (px > thresholdx)){
          point.pt.x=  px;
          point.pt.y = py;
          points_out.push_back(point); // Anexar punto umbral al vector de puntos de salida
          }

          
        }
    }

    void checkStatus(vector<Point2f> &points, vector<uchar> &status, vector<Point2f> &pointsOut){
      uchar status_ok;
      std:vector<Point2f>::const_iterator pointerData;
      pointerData = points.begin();
      for (std::vector<uchar>::const_iterator i = status.begin(); i != status.end(); ++i)
        {
          status_ok = *i;
          if (status_ok == 1){
            pointsOut.push_back(*pointerData); // Anexar el punto con status OK al vector de salida
          }
          pointerData++;
        }
    }

//  g++ -g -o Visual_ODO_AKAZE.out Visual_ODO_AKAZE.cpp `pkg-config opencv --cflags --libs`
// ./Visual_ODO_AKAZE.out ../../../../../dataset/odometry/00/image_2/