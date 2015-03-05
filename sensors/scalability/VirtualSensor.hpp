#ifndef VIRTUAL_SENSOR_HPP
#define VIRTUAL_SENSOR_HPP
/*****************************************************************************/
// Definition for virtual sensors, it's one-one map to the user space library
// Sensor types
#define SENSOR_TYPE_GESTURE_FLICK           100
#define SENSOR_TYPE_GESTURE                 101
#define SENSOR_TYPE_PHYSICAL_ACTIVITY       102
#define SENSOR_TYPE_TERMINAL                103
#define SENSOR_TYPE_AUDIO_CLASSIFICATION    104
#define SENSOR_TYPE_PEDOMETER               105
#define SENSOR_TYPE_SHAKE                   106
#define SENSOR_TYPE_SIMPLE_TAPPING          108
#define SENSOR_TYPE_MOVE_DETECT             109

// Sensor event types
#define SHIFT_GESTURE_FLICK         4
#define SHIFT_GESTURE               5
#define SHIFT_PHYSICAL_ACTIVITY     6
#define SHIFT_TERMINAL              7
#define SHIFT_AUDIO_CLASSIFICATION  8
#define SHIFT_SHAKE_TITLT           9
#define SHIFT_SIMPLE_TAPPING        10
#define SHIFT_MOVE_DETECT           11

#define SENSOR_EVENT_TYPE_GESTURE_LEFT_FLICK        (1 << SHIFT_GESTURE_FLICK | 1)
#define SENSOR_EVENT_TYPE_GESTURE_RIGHT_FLICK       (1 << SHIFT_GESTURE_FLICK | 2)
#define SENSOR_EVENT_TYPE_GESTURE_UP_FLICK          (1 << SHIFT_GESTURE_FLICK | 3)
#define SENSOR_EVENT_TYPE_GESTURE_DOWN_FLICK        (1 << SHIFT_GESTURE_FLICK | 4)
#define SENSOR_EVENT_TYPE_GESTURE_LEFT_FLICK_TWICE  (1 << SHIFT_GESTURE_FLICK | 5)
#define SENSOR_EVENT_TYPE_GESTURE_RIGHT_FLICK_TWICE (1 << SHIFT_GESTURE_FLICK | 6)
#define SENSOR_EVENT_TYPE_GESTURE_NO_FLICK          (1 << SHIFT_GESTURE_FLICK | 7)

#define SENSOR_EVENT_TYPE_GESTURE_NUMBER_ONE            (1 << SHIFT_GESTURE | 1)
#define SENSOR_EVENT_TYPE_GESTURE_NUMBER_TWO            (1 << SHIFT_GESTURE | 2)
#define SENSOR_EVENT_TYPE_GESTURE_NUMBER_THREE          (1 << SHIFT_GESTURE | 3)
#define SENSOR_EVENT_TYPE_GESTURE_NUMBER_FOUR           (1 << SHIFT_GESTURE | 4)
#define SENSOR_EVENT_TYPE_GESTURE_NUMBER_FIVE           (1 << SHIFT_GESTURE | 5)
#define SENSOR_EVENT_TYPE_GESTURE_NUMBER_SIX            (1 << SHIFT_GESTURE | 6)
#define SENSOR_EVENT_TYPE_GESTURE_NUMBER_SEVEN          (1 << SHIFT_GESTURE | 7)
#define SENSOR_EVENT_TYPE_GESTURE_NUMBER_EIGHT          (1 << SHIFT_GESTURE | 8)
#define SENSOR_EVENT_TYPE_GESTURE_NUMBER_NINE           (1 << SHIFT_GESTURE | 9)
#define SENSOR_EVENT_TYPE_GESTURE_NUMBER_ZERO           (1 << SHIFT_GESTURE | 10)
#define SENSOR_EVENT_TYPE_GESTURE_NUMBER_EAR_TOUCH      (1 << SHIFT_GESTURE | 11)
#define SENSOR_EVENT_TYPE_GESTURE_NUMBER_EAR_TOUCH_BACK (1 << SHIFT_GESTURE | 12)

#define SENSOR_EVENT_TYPE_PHYSICAL_ACTIVITY_WALKING     (1 << SHIFT_PHYSICAL_ACTIVITY | 1)
#define SENSOR_EVENT_TYPE_PHYSICAL_ACTIVITY_RUNNING     (1 << SHIFT_PHYSICAL_ACTIVITY | 2)
#define SENSOR_EVENT_TYPE_PHYSICAL_ACTIVITY_SEDENTARY   (1 << SHIFT_PHYSICAL_ACTIVITY | 3)
#define SENSOR_EVENT_TYPE_PHYSICAL_ACTIVITY_RANDOM      (1 << SHIFT_PHYSICAL_ACTIVITY | 4)
#define SENSOR_EVENT_TYPE_PHYSICAL_ACTIVITY_BIKING      (1 << SHIFT_PHYSICAL_ACTIVITY | 5)
#define SENSOR_EVENT_TYPE_PHYSICAL_ACTIVITY_DRIVING     (1 << SHIFT_PHYSICAL_ACTIVITY | 6)

#define SENSOR_EVENT_TYPE_TERMINAL_FACE_UP              (1 << SHIFT_TERMINAL | 1)
#define SENSOR_EVENT_TYPE_TERMINAL_FACE_DOWN            (1 << SHIFT_TERMINAL | 2)
#define SENSOR_EVENT_TYPE_TERMINAL_PORTRAIT_UP          (1 << SHIFT_TERMINAL | 3)
#define SENSOR_EVENT_TYPE_TERMINAL_PORTRAIT_DOWN        (1 << SHIFT_TERMINAL | 4)
#define SENSOR_EVENT_TYPE_TERMINAL_HORIZONTAL_UP        (1 << SHIFT_TERMINAL | 5)
#define SENSOR_EVENT_TYPE_TERMINAL_HORIZONTAL_DOWN      (1 << SHIFT_TERMINAL | 6)
#define SENSOR_EVENT_TYPE_TERMINAL_UNKNOWN              (1 << SHIFT_TERMINAL | 7)

#define SENSOR_EVENT_TYPE_AUDIO_CLASSIFICATION_CROWD            (1 << SHIFT_AUDIO_CLASSIFICATION | 1)
#define SENSOR_EVENT_TYPE_AUDIO_CLASSIFICATION_SOFT_MUSIC       (1 << SHIFT_AUDIO_CLASSIFICATION | 2)
#define SENSOR_EVENT_TYPE_AUDIO_CLASSIFICATION_MECHANICAL       (1 << SHIFT_AUDIO_CLASSIFICATION | 3)
#define SENSOR_EVENT_TYPE_AUDIO_CLASSIFICATION_MOTION           (1 << SHIFT_AUDIO_CLASSIFICATION | 4)
#define SENSOR_EVENT_TYPE_AUDIO_CLASSIFICATION_MALE_SPEECH      (1 << SHIFT_AUDIO_CLASSIFICATION | 5)
#define SENSOR_EVENT_TYPE_AUDIO_CLASSIFICATION_FEMALE_SPEECH    (1 << SHIFT_AUDIO_CLASSIFICATION | 6)
#define SENSOR_EVENT_TYPE_AUDIO_CLASSIFICATION_SILENT           (1 << SHIFT_AUDIO_CLASSIFICATION | 7)
#define SENSOR_EVENT_TYPE_AUDIO_CLASSIFICATION_UNKNOWN          (1 << SHIFT_AUDIO_CLASSIFICATION | 8)

#define SENSOR_EVENT_TYPE_SHAKE         (1 << SHIFT_SHAKE_TITLT | 1)
#define SENSOR_EVENT_TYPE_SIMPLE_TAPPING_DOUBLE_TAPPING         (1 << SHIFT_SIMPLE_TAPPING | 1)

#define SENSOR_EVENT_TYPE_MOVE_DETECT_STILL             (1 << SHIFT_MOVE_DETECT | 1)
#define SENSOR_EVENT_TYPE_MOVE_DETECT_SLIGHT            (1 << SHIFT_MOVE_DETECT | 2)
#define SENSOR_EVENT_TYPE_MOVE_DETECT_MOVE              (1 << SHIFT_MOVE_DETECT | 3)

// Sensor delay types
#define SENSOR_DELAY_TYPE_PHYSICAL_ACTIVITY_INSTANT     (((1 << 3) + 1) * 1000)
#define SENSOR_DELAY_TYPE_PHYSICAL_ACTIVITY_QUICK       (((1 << 3) + 2) * 1000)
#define SENSOR_DELAY_TYPE_PHYSICAL_ACTIVITY_NORMAL      (((1 << 3) + 3) * 1000)
#define SENSOR_DELAY_TYPE_PHYSICAL_ACTIVITY_STATISTIC   (((1 << 3) + 4) * 1000)

#define SENSOR_DELAY_TYPE_AUDIO_CLASSIFICATION_INSTANT  (((1 << 3) + 5) * 1000)
#define SENSOR_DELAY_TYPE_AUDIO_CLASSIFICATION_SHORT    (((1 << 3) + 6) * 1000)
#define SENSOR_DELAY_TYPE_AUDIO_CLASSIFICATION_MEDIUM   (((1 << 3) + 7) * 1000)
#define SENSOR_DELAY_TYPE_AUDIO_CLASSIFICATION_LONG     (((1 << 3) + 8) * 1000)

#define SENSOR_DELAY_TYPE_PEDOMETER_INSTANT         (((1 << 3) + 9) * 1000)
#define SENSOR_DELAY_TYPE_PEDOMETER_QUICK           (((1 << 3) + 10) * 1000)
#define SENSOR_DELAY_TYPE_PEDOMETER_NORMAL          (((1 << 3) + 11) * 1000)
#define SENSOR_DELAY_TYPE_PEDOMETER_STATISTIC       (((1 << 3) + 12) * 1000)

/*****************************************************************************/

// PSH session related
// gesture sensor
#define GS_BUF_SIZE     512     /* accel & gyro buffer size */
#define PX_BUF_SIZE     512     /* proximity buffer size */
#define GS_SAMPLE_RATE  50      /* accel & gyro sampling rate, Hz */
#define GS_BUF_DELAY    0       /* accel & gyro buffer delay, ms */
#define PX_SAMPLE_RATE  5       /* proximity sampling rate, Hz */
#define PX_BUF_DELAY    0       /* proximity buffer delay, ms */
#define GS_DATA_LENGTH  6       /* length of input single data */
#define INVALID_GESTURE_RESULT -1

// gesture flick sensor
#define GF_SAMPLE_RATE  20      /* gesture flick sampling rate, Hz */
#define GF_BUF_DELAY    0       /* gesture flick buffer delay, ms */
#define PSH_SESSION_NOT_OPENED NULL

typedef enum { /* definition flick gestures value */
    NO_FLICK = 0,
    LEFT_FLICK,
    RIGHT_FLICK,
    UP_FLICK,
    DOWN_FLICK,
    LEFT_TWICE,
    RIGHT_TWICE,
    FLICK_GESTURES_MAX = 0x7FFFFFFF
} FlickGestures;

// physical activity sensor
#define SLEEP_ON_FAIL_USEC 500000

#define PA_INTERVAL (static_cast<float>(5.12))
#define PA_QUICK (static_cast<float>(5.12))
#define PA_NORMAL (static_cast<float>(40.96))
#define PA_STATISTIC (static_cast<float>(163.84))


// pedometer sensor
#define SLEEP_ON_FAIL_USEC 500000
#define PEDO_RATE 1
#define PEDO_BUFD 0
/*This flag makes pedometer context
  continue computing but not report
  when the screen is off
 */
#define PEDO_FLAG NO_STOP_NO_REPORT_WHEN_SCREEN_OFF

#define PEDO_QUICK 4
#define PEDO_NORMAL 32
#define PEDO_STATISTIC 256

// shake sensor
#define SHAKING_BUF_SIZE            512
#define SHAKING_SAMPLE_RATE         20
#define SHAKING_BUF_DELAY           0
#define SHAKE_SEN_MEDIUM            1
#define SHAKING 1

// simple tapping sensor
#define STAP_BUF_SIZE            512
#define STAP_SAMPLE_RATE         20
#define STAP_BUF_DELAY           0
#define STAP_DEFAULT_LEVEL       0
#define DOUBLE_TAPPING 2
#define INVALID_EVENT -1

// move detect sensor
#define MD_UNKNOW 0
#define MD_MOVE 1
#define MD_SLIGHT 2
#define MD_STILL 3

// terminal sensor
#define TERM_RATE 20
#define TERM_DELAY (1000/TERM_RATE)
#define FACE_UNKNOWN 0
#define FACE_UP 1
#define FACE_DOWN 2
#define ORIENTATION_UNKNOWN 0
#define HORIZONTAL_UP 1
#define HORIZONTAL_DOWN 2
#define PORTRAIT_UP 3
#define PORTRAIT_DOWN 4

// some common definitions
#define THREAD_NOT_STARTED 0
#define PIPE_NOT_OPENED -1
#define PSH_SESSION_NOT_OPENED NULL
#endif
