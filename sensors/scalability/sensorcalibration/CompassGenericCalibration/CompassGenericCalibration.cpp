#include <string.h>
#include <cutils/log.h>
#include "CompassGenericCalibration.h"
#include "mat.h"

using namespace android;

#ifdef SENSOR_DBG
#define  D(...)  LOGD(__VA_ARGS__)
#else
#define  D(...)  ((void)0)
#endif

#define  E(...)  LOGE(__VA_ARGS__)
#define  I(...)  LOGI(__VA_ARGS__)

#define DS_SIZE 48
#define EPSILON  0.000000001
#define PI 3.1416

typedef struct {
    /* hard iron offsets */
    mat<double, 1, 3> offset;

    /* soft iron matrix*/
    mat<double, 3, 3> w_invert;

    /* geomagnetic strength */
    double bfield;
} CompassCalData;

typedef mat<double, 3, DS_SIZE> mat_input_t;

#define MIN_DIFF 1.5f
#define MAX_SQR_ERR 2.0f
#define LOOKBACK_COUNT 6

static float select_points[DS_SIZE][3];
static int select_point_count = 0;

#ifdef DBG_RAW_DATA
#define MAX_RAW_DATA_COUNT 2000
static FILE *raw_data = NULL;
static FILE *raw_data_selected = NULL;
static int raw_data_count = 0;
int file_no = 0;
#endif

static CompassCalData cal_data;
static int g_caled = 0;

// Given an real symmetric 3x3 matrix A, compute the eigenvalues
// ref: http://en.wikipedia.org/wiki/Eigenvalue_algorithm
static void compute_eigenvalues(const mat<double, 3, 3> &A, double &eig1, double &eig2, double &eig3)
{
    double p = A[1][0] * A[1][0] + A[2][0] * A[2][0] + A[2][1] * A[2][1];

    // check if the matrix is diagonal
    if (p < EPSILON) {
        eig1 = A[0][0];
        eig2 = A[1][1];
        eig3 = A[2][2];
        return;
    }

    double q = (A[0][0] + A[1][1] + A[2][2]) / 3;
    double temp1 = A[0][0] - q;
    double temp2 = A[1][1] - q;
    double temp3 = A[2][2] - q;
    p = temp1 * temp1 + temp2 * temp2 + temp3 * temp3 + 2 * p;
    p = sqrt(p / 6);

    mat<double, 3, 3> B = A;
    B[0][0] -= q;
    B[1][1] -= q;
    B[2][2] -= q;
    B = (1/p) * B;

    double r = (B[0][0] * B[1][1] * B[2][2] + B[1][0] * B[2][1] * B[0][2]
        + B[2][0] * B[0][1] * B[1][2] - B[2][0] * B[1][1] * B[0][2]
        - B[0][0] * B[2][1] * B[1][2] - B[1][0] * B[0][1] * B[2][2]) / 2;

    double phi;
    if (r <= -1.0)
        phi = PI/3;
    else if (r >= 1.0)
        phi = 0;
    else
        phi = acos(r) / 3;

    eig3 = q + 2 * p * cos(phi);
    eig1 = q + 2 * p * cos(phi + 2*PI/3);
    eig2 = 3 * q - eig1 - eig3;
}


static vec<double, 3>
calc_evector(const mat<double, 3, 3> &A, double eig)
{
    mat<double, 3, 3> H = A;
    H[0][0] -= eig;
    H[1][1] -= eig;
    H[2][2] -= eig;

    mat<double, 2, 2> X;
    X[0][0] = H[1][1];
    X[1][0] = H[2][1];
    X[0][1] = H[1][2];
    X[1][1] = H[2][2];
    X = invert(X);

    double temp1 = X[0][0] * (-H[0][1]) + X[1][0] * (-H[0][2]);
    double temp2 = X[0][1] * (-H[0][1]) + X[1][1] * (-H[0][2]);
    double norm = sqrt(1 + temp1 * temp1 + temp2 * temp2);

    vec<double, 3> evector;
    evector[0] = 1.0 / norm;
    evector[1] = temp1 / norm;
    evector[2] = temp2 / norm;
    return evector;
}

bool ellipsoid_fit(const mat_input_t &M, mat<double, 1, 3> &offset,
    mat<double, 3, 3> &w_invert, double &bfield)
{
    int i, j;
    mat<double, 9, DS_SIZE> H;
    mat<double, 1, DS_SIZE> W;

    for (i = 0; i < DS_SIZE; ++i) {
        W[0][i] = M[0][i] * M[0][i];
        H[0][i] = M[0][i];
        H[1][i] = M[1][i];
        H[2][i] = M[2][i];
        H[3][i] = -1 * M[0][i] * M[1][i];
        H[4][i] = -1 * M[0][i] * M[2][i];
        H[5][i] = -1 * M[1][i] * M[2][i];
        H[6][i] = -1 * M[1][i] * M[1][i];
        H[7][i] = -1 * M[2][i] * M[2][i];
        H[8][i] = 1;
    }

    mat<double, DS_SIZE, 9> H_trans = transpose(H);
    mat<double, 9, 9> P_temp1 = invert(H_trans * H);
    mat<double, DS_SIZE, 9> P_temp2 = P_temp1 * H_trans;
    mat<double, 1, 9> P = P_temp2 * W;
    mat<double, 3, 3> temp1;
    temp1[0][0] = 2;
    temp1[1][0] = P[0][3];
    temp1[2][0] = P[0][4];
    temp1[0][1] = P[0][3];
    temp1[1][1] = 2 * P[0][6];
    temp1[2][1] = P[0][5];
    temp1[0][2] = P[0][4];
    temp1[1][2] = P[0][5];
    temp1[2][2] = 2 * P[0][7];

    mat<double, 1, 3> temp2;
    temp2[0][0] = P[0][0];
    temp2[0][1] = P[0][1];
    temp2[0][2] = P[0][2];

    offset = invert(temp1) * temp2;
    double off_x = offset[0][0];
    double off_y = offset[0][1];
    double off_z = offset[0][2];

    mat<double, 3, 3> A;
    A[0][0] = 1.0 / (P[0][8] + off_x * off_x + P[0][6] * off_y * off_y
            + P[0][7] * off_z * off_z + P[0][3] * off_x * off_y
            + P[0][4] * off_x * off_z + P[0][5] * off_y * off_z);

    A[1][0] = P[0][3] * A[0][0] / 2;
    A[2][0] = P[0][4] * A[0][0] / 2;
    A[2][1] = P[0][5] * A[0][0] / 2;
    A[1][1] = P[0][6] * A[0][0];
    A[2][2] = P[0][7] * A[0][0];
    A[1][2] = A[2][1];
    A[0][1] = A[1][0];
    A[0][2] = A[2][0];

    double eig1, eig2, eig3;
    compute_eigenvalues(A, eig1, eig2, eig3);

    mat<double, 3, 3> sqrt_evals;
    sqrt_evals[0][0] = sqrt(eig1);
    sqrt_evals[1][0] = 0;
    sqrt_evals[2][0] = 0;
    sqrt_evals[0][1] = 0;
    sqrt_evals[1][1] = sqrt(eig2);
    sqrt_evals[2][1] = 0;
    sqrt_evals[0][2] = 0;
    sqrt_evals[1][2] = 0;
    sqrt_evals[2][2] = sqrt(eig3);

    mat<double, 3, 3> evecs;
    evecs << calc_evector(A, eig1) << calc_evector(A, eig2) << calc_evector(A, eig3);

    w_invert = transpose(evecs * sqrt_evals * transpose(evecs));
    bfield = pow(sqrt(1/eig1) * sqrt(1/eig2) * sqrt(1/eig3), 1.0/3.0);
    w_invert = w_invert * bfield;

    return true;
}

/* reset calibration algorithm */
static void reset()
{
    select_point_count = 0;
    for (int i = 0; i < DS_SIZE; ++i)
        for (int j=0; j < 3; ++j)
            select_points[i][j] = 0;
}

void CompassCal_init(FILE *calDataFile)
{
#ifdef DBG_RAW_DATA
    if (raw_data) {
        fclose(raw_data);
        raw_data = NULL;
    }

    if (raw_data_selected) {
        fclose(raw_data_selected);
        raw_data_selected = NULL;
    }

    // open raw data file
    char path[64];
    snprintf(path, 64, "/data/raw_compass_data_full_%d.txt", file_no);
    raw_data = fopen(path, "w+");
    snprintf(path, 64, "/data/raw_compass_data_selected_%d.txt", file_no);
    raw_data_selected = fopen(path, "w+");
    ++file_no;
    raw_data_count = 0;
#endif

    reset();

    g_caled = 0;
    double x, y, z, w11, w12, w13, w21, w22, w23, w31, w32, w33, bfield;
    if (calDataFile != NULL) {
        int ret = fscanf(calDataFile, "%d %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf",
            &g_caled, &x, &y, &z, &w11, &w12, &w13, &w21, &w22, &w23,
            &w31, &w32, &w33, &bfield);
        if (ret != 14)
           g_caled = 0;
    }

    if (g_caled) {
        cal_data.offset[0][0] = x;
        cal_data.offset[0][1] = y;
        cal_data.offset[0][2] = z;

        cal_data.w_invert[0][0] = w11;
        cal_data.w_invert[1][0] = w12;
        cal_data.w_invert[2][0] = w13;
        cal_data.w_invert[0][1] = w21;
        cal_data.w_invert[1][1] = w22;
        cal_data.w_invert[2][1] = w23;
        cal_data.w_invert[0][2] = w31;
        cal_data.w_invert[1][2] = w32;
        cal_data.w_invert[2][2] = w33;

        cal_data.bfield = bfield;

        D("CompassCalibration: load old data, caldata: %f %f %f %f %f %f %f %f %f %f %f %f %f",
            cal_data.offset[0][0], cal_data.offset[0][1], cal_data.offset[0][2],
            cal_data.w_invert[0][0], cal_data.w_invert[1][0],cal_data.w_invert[2][0],cal_data.w_invert[0][1],
            cal_data.w_invert[1][1], cal_data.w_invert[2][1],cal_data.w_invert[0][2],cal_data.w_invert[1][2],
            cal_data.w_invert[2][2], cal_data.bfield);
    } else {
        cal_data.offset[0][0] = 0;
        cal_data.offset[0][1] = 0;
        cal_data.offset[0][2] = 0;

        cal_data.w_invert[0][0] = 1;
        cal_data.w_invert[1][0] = 0;
        cal_data.w_invert[2][0] = 0;
        cal_data.w_invert[0][1] = 0;
        cal_data.w_invert[1][1] = 1;
        cal_data.w_invert[2][1] = 0;
        cal_data.w_invert[0][2] = 0;
        cal_data.w_invert[1][2] = 0;
        cal_data.w_invert[2][2] = 1;

        cal_data.bfield = 0;
    }
}

void CompassCal_storeResult(FILE *calDataFile)
{
    if (!calDataFile)
        return;

    int ret = fprintf(calDataFile, "%d %f %f %f %f %f %f %f %f %f %f %f %f %f\n", g_caled,
            cal_data.offset[0][0], cal_data.offset[0][1], cal_data.offset[0][2],
            cal_data.w_invert[0][0], cal_data.w_invert[1][0], cal_data.w_invert[2][0],
            cal_data.w_invert[0][1], cal_data.w_invert[1][1], cal_data.w_invert[2][1],
            cal_data.w_invert[0][2], cal_data.w_invert[1][2], cal_data.w_invert[2][2],
            cal_data.bfield);
    if (ret < 0) {
        E("CompassSensor - Store calibration data failed: %s", strerror(ret));
    }
}

/* return 0 reject value, return 1 accept value. */
int CompassCal_collectData(float rawMagX, float rawMagY, float rawMagZ, long currentTimeMSec)
{
    float data[3] = {rawMagX, rawMagY, rawMagZ};

#ifdef DBG_RAW_DATA
    if (raw_data && raw_data_count < MAX_RAW_DATA_COUNT) {
        fprintf(raw_data, "%f %f %f %d\n", (double)rawMagX, (double)rawMagY,
                 (double)rawMagZ, (int)currentTimeMSec);
        raw_data_count++;
    }

    if (raw_data && raw_data_count >= MAX_RAW_DATA_COUNT) {
        fclose(raw_data);
        raw_data = NULL;
    }
#endif

    // For the current point to be accepted, each x/y/z value must be different enough
    // to the last several collected points
    if (select_point_count > 0 && select_point_count < DS_SIZE) {
        int lookback = LOOKBACK_COUNT < select_point_count ? LOOKBACK_COUNT : select_point_count;
        for (int index = 0; index < lookback; ++index){
            for (int j = 0; j < 3; ++j) {
                if (fabsf(data[j] - select_points[select_point_count-1-index][j]) < MIN_DIFF) {
                    D("CompassCalibration:point reject: [%f,%f,%f], selected_count=%d",
                       (double)data[0], (double)data[1], (double)data[2], select_point_count);
                        return 0;
                }
            }
        }
    }

    if (select_point_count < DS_SIZE) {
        memcpy(select_points[select_point_count], data, sizeof(float) * 3);
        ++select_point_count;
        D("CompassCalibration:point collected [%f,%f,%f], selected_count=%d",
            (double)data[0], (double)data[1], (double)data[2], select_point_count);
#ifdef DBG_RAW_DATA
        if (raw_data_selected) {
            fprintf(raw_data_selected, "%f %f %f\n", (double)data[0], (double)data[1], (double)data[2]);
        }
#endif
    }
   return 1;
}


// use second data set to calculate square error
double calc_square_err(const CompassCalData &data)
{
    double err = 0;
    mat<double, 1, 3> raw, result;
    for (int i = 0; i < DS_SIZE; ++i) {
        raw[0][0] = select_points[i][0];
        raw[0][1] = select_points[i][1];
        raw[0][2] = select_points[i][2];
        result = data.w_invert * (raw - data.offset);
         double diff = sqrt(result[0][0] * result[0][0] + result[0][1] * result[0][1]
             + result[0][2] * result[0][2]) - data.bfield;
         err += diff * diff;
    }
    err /= DS_SIZE;
    return err;
}

/* check if calibration complete */
int CompassCal_readyCheck()
{
    mat_input_t mat;

    if (select_point_count < DS_SIZE)
        return g_caled;

    // enough points have been collected, do the ellipsoid calibration
    for (int i = 0; i < DS_SIZE; ++i) {
        mat[0][i] = select_points[i][0];
        mat[1][i] = select_points[i][1];
        mat[2][i] = select_points[i][2];
    }

    /* check if result is good */
    CompassCalData new_cal_data;
    if (ellipsoid_fit(mat, new_cal_data.offset,
                      new_cal_data.w_invert, new_cal_data.bfield)) {
        double err_new = calc_square_err(new_cal_data);
        if (err_new < MAX_SQR_ERR) {
            double err = calc_square_err(cal_data);
            if (err_new < err) {
              // new cal_data is better, so we switch to the new
              cal_data = new_cal_data;
              g_caled = 1;
              D("CompassCalibration: ready check success, caldata: %f %f %f %f %f %f %f %f %f %f %f %f %f, err %f",
                cal_data.offset[0][0], cal_data.offset[0][1], cal_data.offset[0][2], cal_data.w_invert[0][0],
                cal_data.w_invert[1][0], cal_data.w_invert[2][0], cal_data.w_invert[0][1],cal_data.w_invert[1][1],
                cal_data.w_invert[2][1], cal_data.w_invert[0][2], cal_data.w_invert[1][2], cal_data.w_invert[2][2],
                cal_data.bfield, err_new);
            }
        }
    }

    reset();
    return g_caled;
}

void CompassCal_computeCal(float rawX, float rawY, float rawZ,
                float *resultX, float *resultY, float *resultZ)
{
  if (!g_caled)
      return;

    mat<double, 1, 3> raw, result;
    raw[0][0] = rawX;
    raw[0][1] = rawY;
    raw[0][2] = rawZ;

    result = cal_data.w_invert * (raw - cal_data.offset);
    *resultX = (float) result[0][0];
    *resultY = (float) result[0][1];
    *resultZ = (float) result[0][2];
}
