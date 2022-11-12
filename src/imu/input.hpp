#ifndef IMU_INPUT_HPP
#define IMU_INPUT_HPP

class imu
{
public:
    static bool safe_mode;

    static void init();
    static void enable();
    static void disable();
    static void A(float &ax, float &ay, float &az);
    static void G(float &gx, float &gy, float &gz);

    static inline const float &Ax() { return _Ax; }
    static inline const float &Ay() { return _Ay; }
    static inline const float &Az() { return _Az; }
    static inline const float &Gx() { return _Gx; }
    static inline const float &Gy() { return _Gy; }
    static inline const float &Gz() { return _Gz; }
private:
    static float _Ax, _Ay, _Az;
    static float _Gx, _Gy, _Gz;
    static float A_sample_rate, G_sample_rate;
    static bool enabled, initialized;
    static void worker();
};

#endif
