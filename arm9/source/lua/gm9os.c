#include "gm9os.h"
#include "timer.h"
#include "rtc.h"
#include "ui.h"

u64 osclock;

static inline bool isLeapYear(u32 year) {
    return (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)); 
}

size_t getWeekday(bool abbreviated, char* out, u8 weekday) {
    if (abbreviated) {
        switch(weekday) {
            case 1:
                strcat(out, "Mon");
                break;
            case 2:
                strcat(out, "Tue");
                break;
            case 3:
                strcat(out, "Wed");
                break;
            case 4:
                strcat(out, "Thu");
                break;
            case 5:
                strcpy(out, "Fri");
                break;
            case 6:
                strcat(out, "Sat");
                break;
            case 7:
                strcat(out, "Sun");
                break;
            default: 
                strcat(out, "");
                return 0;
        }
        return 3;
    }
    switch(weekday) {
        case 1:
            strcat(out, "Monday");
            return 6;
        case 2:
            strcat(out, "Tuesday");
            return 7;
        case 3:
            strcat(out, "Wednesday");
            return 9;
        case 4:
            strcat(out, "Thursday");
            return 8;
        case 5:
            strcpy(out, "Friday");
            return 6;
        case 6:
            strcat(out, "Saturday");
            return 8;
        case 7:
            strcat(out, "Sunday");
            return 6;
        default: 
            strcat(out, "");
            return 0;
    }
    return 0;
    
}

size_t getMonthName(bool abbreviated, char* out, u8 month) {
    if (abbreviated) {
        switch(month) {
            case 1:
                strcat(out, "Jan");
                break;
            case 2:
                strcat(out, "Feb");
                break;
            case 3:
                strcat(out, "Mar");
                break;
            case 4:
                strcat(out, "Apr");
                break;
            case 5:
                strcat(out, "May");
                break;
            case 6:
                strcat(out, "Jun");
                break;
            case 7:
                strcat(out, "Jul");
                break;
            case 8:
                strcat(out, "Aug");
                break;
            case 9:
                strcat(out, "Sep");
                break;
            case 10:
                strcat(out, "Oct");
                break;
            case 11:
                strcat(out, "Nov");
                break;
            case 12:
                strcat(out, "Dec");
                break; 
            default:
                strcat(out, "");
                return 0;
        }
        return 3;
    }
    switch(month) {
        case 1:
            strcat(out, "January");
            return 7;
        case 2:
            strcat(out, "February");
            return 8;
        case 3:
            strcat(out, "March");
            return 5;
        case 4:
            strcat(out, "April");
            return 5;
        case 5:
            strcat(out, "May");
            return 3;
        case 6:
            strcat(out, "Juny");
            return 4;
        case 7:
            strcat(out, "July");
            return 4;
        case 8:
            strcat(out, "August");
            return 6;
        case 9:
            strcat(out, "September");
            return 9;
        case 10:
            strcat(out, "October");
            return 7;
        case 11:
            strcat(out, "November");
            return 8;
        case 12:
            strcat(out, "December");
            return 8; 
        default:
            strcat(out, "");
            return 0;
    }
    return 0;
}

u16 getDaysMonths(u32 months, u8 years) {
    u8 daysInMonth[12] = {31, isLeapYear(2000 + years) ? 29 : 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}; //is this ?: bad practice?
    u16 ret;
    for (u32 month = 0; month < months - 1; month++) {
        ret += daysInMonth[month];
    }
    return ret;
}

bool my_strftime(char* _out, size_t _maxsize, char* str, DsTime *dstime) { //my refers to github.com/Gruetzig
    size_t strl = strlen(str);
    size_t outpos = 0;
    char out[_maxsize+10];
    memset(out, 0, _maxsize+10);
    u8 minute, hour, day, month, year, weekday, second, weeknumber;
    u16 currentday, nextsunday, fyear;
    char numbuf[3], numbuf2[5], numbuf3[9], numnum1[3], numnum2[3], numnum3[3];
    if (!is_valid_dstime(dstime)) {
        return false;
    }
    for (size_t i = 0;i<strl;i++) {
        if (str[i] == '%') {
            i++;
            switch(str[i]) {
                case 'a':
                    outpos += getWeekday(true, out, DSTIMEGET(dstime, weekday));
                    break;
                case 'A':
                    outpos += getWeekday(false, out, DSTIMEGET(dstime, weekday));
                    break;
                case 'b':
                    outpos += getMonthName(true, out, DSTIMEGET(dstime, bcd_M));
                    break;
                case 'B':
                    outpos += getMonthName(false, out, DSTIMEGET(dstime, bcd_M));
                    break;
                case 'c':
                    char buf[100];
                    my_strftime(buf, _maxsize-outpos-10, "%a %b %d %X %Y", dstime);
                    strcat(out, buf);
                    outpos += strlen(buf);
                    break;
                case 'd':
                    day = DSTIMEGET(dstime, bcd_D);
                    if (day < 10) {
                        sprintf(numbuf, "0%d", day);
                    } else {
                        sprintf(numbuf, "%d", day);
                    }
                    strcat(out, numbuf);
                    outpos += 2;
                    break;
                case 'H':
                    hour = DSTIMEGET(dstime, bcd_h);
                    if (day < 10) {
                        sprintf(numbuf, "0%d", hour);
                    } else {
                        sprintf(numbuf, "%d", hour);
                    }
                    strcat(out, numbuf);
                    outpos += 2;
                    break;
                case 'I':
                    hour = DSTIMEGET(dstime, bcd_h);
                    if (hour > 12) {
                        hour = hour - 12;
                    }
                    if (!hour) {
                        hour = 12;
                    }
                    if (hour < 10) {
                        sprintf(numbuf, "0%d", hour);
                    } else {
                        sprintf(numbuf, "%d", hour);
                    }
                    strcat(out, numbuf);
                    outpos += 2;
                    break;
                case 'j':
                    currentday = getDaysMonths(DSTIMEGET(dstime, bcd_M), DSTIMEGET(dstime, bcd_Y))+DSTIMEGET(dstime, bcd_D);
                    if (currentday < 10) {
                        sprintf(numbuf, "00%d", currentday);
                    } else if (currentday < 100) {
                        sprintf(numbuf, "0%d", currentday);
                    } else {
                        sprintf(numbuf, "%d", currentday);
                    }
                    strcat(out, numbuf);
                    outpos += 3;
                    break;
                case 'm':
                    month = DSTIMEGET(dstime, bcd_M);
                    if (month < 10) {
                        sprintf(numbuf, "0%d", month);
                    } else {
                        sprintf(numbuf, "%d", month);
                    }
                    strcat(out, numbuf);
                    outpos += 2;
                    break;
                case 'M':
                    minute = DSTIMEGET(dstime, bcd_m);
                    if (minute < 10) {
                        sprintf(numbuf, "0%d", minute);
                    } else {
                        sprintf(numbuf, "%d", minute);
                    }
                    strcat(out, numbuf);
                    outpos += 2;
                    break;
                case 'p':
                    if (hour >= 12) {
                        strcat(out, "PM");
                    } else {
                        strcat(out, "AM");
                    }
                    outpos += 2;
                    break;
                case 'S':
                    second = DSTIMEGET(dstime, bcd_m);
                    if (second < 10) {
                        sprintf(numbuf, "0%d", second);
                    } else {
                        sprintf(numbuf, "%d", second);
                    }
                    strcat(out, numbuf);
                    outpos += 2;
                    break;
                case 'U': 
                    currentday = getDaysMonths(DSTIMEGET(dstime, bcd_M), DSTIMEGET(dstime, bcd_Y))+DSTIMEGET(dstime, bcd_D);
                    weekday = DSTIMEGET(dstime, weekday);
                    nextsunday = ((7-weekday)+currentday);
                    weeknumber = (nextsunday/7)+1;
                    if (weeknumber < 10) {
                        sprintf(numbuf, "0%d", weeknumber);
                    } else {
                        sprintf(numbuf, "%d", weeknumber);
                    }
                    strcat(out, numbuf);
                    outpos += 2;
                    break;
                case 'w':
                    weekday = DSTIMEGET(dstime, weekday);
                    if (weekday == 7) {
                        weekday = 0;
                    }
                    sprintf(out, "%d", weekday);
                    strcat(out, numbuf);
                    outpos++;
                    break;
                case 'W':
                    currentday = getDaysMonths(DSTIMEGET(dstime, bcd_M), DSTIMEGET(dstime, bcd_Y))+DSTIMEGET(dstime, bcd_D);
                    weekday = DSTIMEGET(dstime, weekday);
                    nextsunday = ((8-weekday)+currentday);
                    weeknumber = (nextsunday/7);
                    if (weeknumber < 10) {
                        sprintf(numbuf, "0%d", weeknumber);
                    } else {
                        sprintf(numbuf, "%d", weeknumber);
                    }
                    strcat(out, numbuf);
                    outpos += 2;
                    break;
                case 'x': 
                    month = DSTIMEGET(dstime, bcd_M);
                    day = DSTIMEGET(dstime, bcd_D);
                    year = DSTIMEGET(dstime, bcd_Y);

                    if (month < 10) {
                        sprintf(numnum1, "0%d", month);
                    } else {
                        sprintf(numnum1, "%d", month);
                    }

                    if (day < 10) {
                        sprintf(numnum2, "0%d", day);
                    } else {
                        sprintf(numnum2, "%d", day);
                    }

                    if (year < 10) {
                        sprintf(numnum3, "0%d", year);
                    } else {
                        sprintf(numnum3, "%d", year);
                    }
                    sprintf(numbuf3, "%s/%s/%s", numnum1, numnum2, numnum3);
                    strcat(out, numbuf3);
                    outpos += 8;
                    break;
                case 'X':
                    hour = DSTIMEGET(dstime, bcd_h);
                    minute = DSTIMEGET(dstime, bcd_m);
                    second = DSTIMEGET(dstime, bcd_s);

                    if (hour < 10) {
                        sprintf(numnum1, "0%d", hour);
                    } else {
                        sprintf(numnum1, "%d", hour);
                    }

                    if (minute < 10) {
                        sprintf(numnum2, "0%d", minute);
                    } else {
                        sprintf(numnum2, "%d", minute);
                    }

                    if (second < 10) {
                        sprintf(numnum3, "0%d", second);
                    } else {
                        sprintf(numnum3, "%d", second);
                    }
                    sprintf(numbuf3, "%s:%s:%s", numnum1, numnum2, numnum3);
                    strcat(out, numbuf3);
                    outpos += 8;
                    break;
                case 'y':
                    year = DSTIMEGET(dstime, bcd_Y);
                    if (year < 10) {
                        sprintf(numbuf, "0%d", year);
                    } else {
                        sprintf(numbuf, "%d", year);
                    }
                    strcat(out, numbuf);
                    outpos += 2;
                    break;
                case 'Y':
                    fyear = DSTIMEGET(dstime, bcd_Y);
                    fyear += 2000;
                    sprintf(numbuf2, "%d", fyear);
                    strcat(out, numbuf2);
                    outpos += 4;
                    break;
                case '%':
                    strcat(out, "%");
                    outpos++;
                    break;
                default:
                    break; //not implemented


            }

        } else {
            out[outpos] = str[i];
            outpos++;
        }
        if (outpos > _maxsize) {
            break;
        }

    }
    strncpy(_out, out, _maxsize);
    return true;
}

u64 calcUnixTime(u8 years, u8 months, u8 days, u8 hours, u8 minutes, u8 seconds) {

    u8 daysInMonth[12] = {31, isLeapYear(2000 + years) ? 29 : 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}; //is this ?: bad practice?
    u32 curdays;
    u64 ret = 0;

    ret += seconds;
    ret += minutes * 60;
    ret += hours * 60 * 60;
    ret += (days - 1) * 24 * 60 * 60;

    for (u16 year = 0; year < years + 30; year++) { //+30 because unix time starting in 1970 but rtc starts in 2000
        if (isLeapYear(2000 + year)) {
            curdays = 366;
        } else {
            curdays = 365;
        }
        ret += curdays * 24 * 60 * 60;
    }

    for (u16 month = 0; month < months - 1; month++) {
        ret += daysInMonth[month] * 24 * 60 * 60;
    }

    return ret;
}

u64 getUnixTimeFromRtc(DsTime *dstime) {

    u8
    seconds = DSTIMEGET(dstime, bcd_s),
    minutes = DSTIMEGET(dstime, bcd_m),
    hours = DSTIMEGET(dstime, bcd_h),
    days = DSTIMEGET(dstime, bcd_D),
    months = DSTIMEGET(dstime, bcd_M),
    years = DSTIMEGET(dstime, bcd_Y);
    return calcUnixTime(years, months, days, hours, minutes, seconds);
    
}

static int os_time(lua_State *L) {
    int args = lua_gettop(L);
    u64 unixtime;
    switch(args) {
        case 0:
            DsTime dstime;
            get_dstime(&dstime);
            unixtime = getUnixTimeFromRtc(&dstime);
            ShowPrompt(false, "%lld", unixtime);
            lua_pushinteger(L, unixtime);
            return 1;
        case 6:
            lua_pushinteger(L, 1);
            lua_gettable(L, 1);
            int year = lua_tointeger(L, -1);
            if (year >= 2000) {
                year -= 2000;
            }
            lua_pop(L, 1);

            lua_pushinteger(L, 2);
            lua_gettable(L, 1);
            int month = lua_tointeger(L, -1);
            lua_pop(L, 1);

            lua_pushinteger(L, 3);
            lua_gettable(L, 1);
            int day = lua_tointeger(L, -1);
            lua_pop(L, 1);

            lua_pushinteger(L, 4);
            lua_gettable(L, 1);
            int hour = lua_tointeger(L, -1);
            lua_pop(L, 1);

            lua_pushinteger(L, 5);
            lua_gettable(L, 1);
            int minute = lua_tointeger(L, -1);
            lua_pop(L, 1);
            
            lua_pushinteger(L, 6);
            lua_gettable(L, 1);
            int second = lua_tointeger(L, -1);
            lua_pop(L, 1);
            
            lua_pushinteger(L, calcUnixTime(year, month, day, hour, minute, second));

            return 1;
        default:
            return luaL_error(L, "not a valid amount of arguments");

    }
}

static int os_date(lua_State *L) {
    int args = lua_gettop(L);
    switch(args) {
        case 0:
            DsTime dstime;
            get_dstime(&dstime);
            char retbuf[100];
            memset(retbuf, 0, 100);
            my_strftime(retbuf, 100, "%c", &dstime);
            lua_pushstring(L, retbuf);
            return 1;

            
        default:
            return luaL_error(L, "not a valid amount of arguments");
    }
}

static int os_clock(lua_State *L) {
    lua_pushinteger(L, timer_ticks(osclock));
    return 1;
}

static const luaL_Reg os[] = {
    {"clock", os_clock},
    {"time", os_time},
    {"date", os_date},
    {NULL, NULL}
};

int gm9lua_open_os(lua_State* L) {
    luaL_newlib(L, os);
    osclock = timer_start();
    return 1;
}