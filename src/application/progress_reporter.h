#ifndef NWAVE_APPLICATION_PROGRESS_REPORTER_H
#define NWAVE_APPLICATION_PROGRESS_REPORTER_H

namespace nwave {

class ProgressReporter {
public:
    virtual ~ProgressReporter() = default;

    virtual void start(int total_frames) = 0;
    virtual void frame_complete(int frame_number) = 0;
    virtual void finish() = 0;
};

} // namespace nwave

#endif // NWAVE_APPLICATION_PROGRESS_REPORTER_H
