//raspberry.hpp
#pragma once
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>
#include <chrono>
#include <opencv2/opencv.hpp>
using namespace std;
using namespace cv;

typedef uint8_t BYTE;
typedef uint8_t GRY;
typedef Vec3b COR;

void erro(string s1="") {
  cerr << s1 << endl;
  exit(1);
}

#define xdebug { string st = "File="+string(__FILE__)+" line="+to_string(__LINE__)+"\n"; cout << st; }
#define xprint(x) { ostringstream os; os << #x " = " << x << '\n'; cout << os.str(); }

double timeSinceEpoch() {
  using namespace std::chrono;
  return duration_cast<duration<double>>( system_clock::now().time_since_epoch() ).count();
}
