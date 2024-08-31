#ifndef NJSCANNER_H_INCLUDE
#define NJSCANNER_H_INCLUDE

#include <Arduino.h>

long intFrom16BaseString(String sourceString) ;
int utf8length(String sourceString);
int numberOfComponentsWithDelimiter(String sourceString, String delimiter);
String componentAtPositionWithDelimiter(String sourceString, int position, String delimiter);
String tagsRemovedString(String sourceString);
String utf8CharStringForCodePoint(long codePoint);

class NJScanner {
  public:
    NJScanner();
    NJScanner(String sourceString);
    void setScanString(String sourceString);
    int scanString(String targetString);
    String scanUpToString(String targetString, boolean skip);
    String scanStringToEnd();
    int scanLocation();
    void setScanLocation(int newLocation);
    boolean isAtEnd();
    
  private:
    int sourceLocation;
    String sourceString;
};

#endif