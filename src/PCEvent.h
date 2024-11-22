#ifndef PCEVENT_H_INCLUDE
#define PCEVENT_H_INCLUDE

#include <Arduino.h>
#include <map>
#include <time.h>

int dayOfWeek(int year, int month, int day);
int numberOfDaysInMonth(int year, int month);
tm tmFromICalDateString(String iCalDateString, float toTimezone);
tm tmFromHTTPDateString(String httpDateString, float toTimezone);
tm convertTimezone(tm timeInfo, float toTimezone);


class PCEvent
{
public:
    PCEvent(String sourceString, float toTimezone);
    PCEvent(int year, int month, int day, String title);
    time_t getTimeT() const;
    int getYear();
    int getMonth();
    int getDay();
    int getDayOfWeek();
    int getHour();
    int getMinute();
    int getSecond();
    String descriptionForDay(boolean isToday);
    double duration();
    String getTitle();
    boolean isHolidayEvent;

    static float defaultTimezone;
    static tm currentTimeinfo;
    static int currentYear;
    static int currentMonth;
    static int currentDay;
    static int nextMonthYear;
    static int nextMonth;
    static void initialize(String rootCA, float timezone, String holidayCacheString = "");
    static void setRootCA(String newRootCA);
    static void setTimeinfo(tm timeinfo);
    static void setHolidayCacheString(String cacheString);
    static String holidayCacheString();
    static boolean isCacheValid();
    static boolean loadICalendar(String urlString, boolean holiday);
    static int numberOfEventsInThisMonth();
    static int numberOfEventsInDayOfThisMonth(int day);
    static std::vector<PCEvent> eventsInDayOfThisMonth(int day);
    static int numberOfHolidaysInDayOfThisMonth(int day);
    static std::vector<PCEvent> holidaysInDayOfThisMonth(int day);
    static std::vector<PCEvent> eventsInNextMonth();

private:
    tm _startTM;
    tm _endTM;
    boolean _isDayEvent;
    float _timezone;
    String _title;

    static String _rootCA;
    static boolean _isCacheValid;
    static std::multimap<int, PCEvent> _eventsInThisMonth;
    static std::multimap<int, PCEvent> _holidaysInThisMonth;
    static std::vector<PCEvent> _eventsInNextMonth;
};

bool operator<(const PCEvent&left, const PCEvent&right) ;
bool operator>(const PCEvent&left, const PCEvent&right) ;

#endif