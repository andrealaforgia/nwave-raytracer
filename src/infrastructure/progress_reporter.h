#ifndef NWAVE_INFRASTRUCTURE_PROGRESS_REPORTER_H
#define NWAVE_INFRASTRUCTURE_PROGRESS_REPORTER_H

#include "application/progress_reporter.h"
#include <chrono>
#include <ostream>
#include <vector>

namespace nwave {

class StreamProgressReporter : public ProgressReporter {
public:
    explicit StreamProgressReporter(std::ostream& output);

    void start(int total_frames) override;
    void frame_complete(int frame_number) override;
    void finish() override;

private:
    std::ostream& output_;
    int total_frames_{0};
    std::chrono::steady_clock::time_point start_time_;
    std::chrono::steady_clock::time_point last_frame_time_;
    std::vector<double> frame_durations_;
};

} // namespace nwave

#endif // NWAVE_INFRASTRUCTURE_PROGRESS_REPORTER_H
