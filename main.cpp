/**
 * Stepper motor usage example arbitrary function.
 *
 * Usage
 * Press and hold a user button during some seconds to increase target stepper motor position and start movement.
 * During stepper motor movement standard output (UART) will show current stepper motor position.
 * If you press and hold the user button again, target position will be being changed but in the invers direction.
 */
#include "mbed.h"
#include "mbed_chrono.h"

#include <chrono>

using namespace std::chrono_literals;
using namespace std::chrono;
using mbed::chrono::microseconds_u32;

static DigitalOut user_led(LED1, 1);

/**
 * Move direction.
 */
enum MoveDirection : int8_t {
    /** Move forward */
    DIR_FORWARD = 1,
    /** neutral position (no movement) */
    DIR_NONE = 0,
    /** move backward */
    DIR_BACKWARD = -1
};

/**
 * Current step instruction.
 */
struct step_instruction_t {
    step_instruction_t() = default;
    step_instruction_t(MoveDirection dir, microseconds_u32 next)
        : dir(dir)
        , next(next)
    {
    }

    /**
       * Movement of current step
       */
    MoveDirection dir;
    /**
       * Delay before next step.
       *
       * Zero value indicates end of movement.
       */
    microseconds_u32 next;
};

/**
 * Stepper motor position.
 *
 * Note: to handle overflow a serial number arithmetic (https://en.wikipedia.org/wiki/Serial_number_arithmetic) is used.
 */
struct position_t {
    position_t() = default;
    position_t(int32_t current, int32_t target)
        : current(current)
        , target(target)
    {
    }

    /** Current stepper motor position */
    int32_t current;
    /** Target stepper motor position */
    int32_t target;
};

typedef Callback<step_instruction_t(const position_t &pos)> CustomStepCallback;

template <typename T>
class SimpleSequenceWrapper : private NonCopyable<SimpleSequenceWrapper<T>> {
public:
    enum ControlFlag : uint8_t {
        /**
         * Stop movement.
         *
         * If this flag is set, current value is ignored and movement is aborted.
         */
        FLAG_STOP = 0x01,
    };

private:
    T _sequence_callback;
    uint32_t _seqeunce_interval_us;

    step_instruction_t _step_instruction;

    uint16_t _step_count;
    int16_t _step_adjustment_count;
    int32_t _steps_to_go_abs;

public:
    /**
     * Constructor.
     *
     * @param sequence_callback sequence callback. Each call should return next sequence value.
     * @param interval interval between sequence samples
     */
    SimpleSequenceWrapper(T sequence_callback, microseconds_u32 interval)
        : _sequence_callback(sequence_callback)
        , _seqeunce_interval_us(interval.count())
        , _step_count(0)
        , _step_adjustment_count(0)
        , _steps_to_go_abs(0)
    {
    }
    ~SimpleSequenceWrapper() = default;

    /**
     * Get next step instruction.
     *
     * @param pos
     * @return
     */
    step_instruction_t next(const position_t &pos)
    {
        int steps_to_go;

        if (_step_count == 0) {
            steps_to_go = _sequence_callback();
            steps_to_go -= pos.current;

            if (steps_to_go == 0) {
                _step_instruction.dir = DIR_NONE;
                _step_instruction.next = microseconds_u32(_seqeunce_interval_us);
                _step_count = 1;
            } else {
                if (steps_to_go > 0) {
                    _step_instruction.dir = DIR_FORWARD;
                } else {
                    _step_instruction.dir = DIR_BACKWARD;
                    steps_to_go = -steps_to_go;
                }
                _step_count = steps_to_go;
                _step_instruction.next = microseconds_u32(_seqeunce_interval_us / steps_to_go);
                _step_adjustment_count = _seqeunce_interval_us / steps_to_go;
                if (_steps_to_go_abs > steps_to_go) {
                    _step_adjustment_count = -_step_adjustment_count;
                    _step_adjustment_count--;
                } else {
                    _step_instruction.next += 1us;
                    _step_adjustment_count++;
                }
            }
            _steps_to_go_abs = steps_to_go;
        }

        _step_count -= 1;
        if (_step_adjustment_count != 0) {
            if (_step_adjustment_count > 0) {
                _step_adjustment_count--;
                if (_step_adjustment_count == 0) {
                    _step_instruction.next -= 1us;
                }
            } else {
                _step_adjustment_count++;
                if (_step_adjustment_count == 0) {
                    _step_instruction.next += 1us;
                }
            }
        }

        return _step_instruction;
    }
};

struct sin_wave_iter_t {
    float a;
    float d_phi;
    float phi;

    static constexpr float DOUBLE_PI = 6.283185307179586f;

    int next()
    {
        phi += d_phi;
        if (phi > DOUBLE_PI) {
            phi -= DOUBLE_PI;
        }
        return a * sinf(phi);
    }
};

int main()
{
    sin_wave_iter_t sin_wave_iter;
    microseconds_u32 dt = 10ms;
    float f = 0.2;
    sin_wave_iter.a = 1000;
    sin_wave_iter.phi = 0.0f;
    sin_wave_iter.d_phi = f * sin_wave_iter.DOUBLE_PI * (dt.count() / 1'000'000.0f);
    SimpleSequenceWrapper<Callback<int()>> seq_wrapper(callback(&sin_wave_iter, &sin_wave_iter_t::next), dt);
    step_instruction_t si;

    si = seq_wrapper.next({ 0, 0 });
    si = seq_wrapper.next({ 0, 0 });

    while (true) {
        for (int i = 0; i < 4; i++) {
            user_led = !user_led;
            ThisThread::sleep_for(100ms);
        }
        ThisThread::sleep_for(600ms);
    }

    return 0;
}
