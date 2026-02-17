#include "infrastructure/progress_reporter.h"
#include <iomanip>
#include <numeric>

namespace nwave {

StreamProgressReporter::StreamProgressReporter(std::ostream& output)
    : output_(output) {}

void StreamProgressReporter::start(int total_frames) {
    total_frames_ = total_frames;
    frame_durations_.clear();
    start_time_ = std::chrono::steady_clock::now();
    last_frame_time_ = start_time_;
}

void StreamProgressReporter::frame_complete(int frame_number) {
    auto now = std::chrono::steady_clock::now();
    double frame_duration = std::chrono::duration<double>(now - last_frame_time_).count();
    last_frame_time_ = now;
    frame_durations_.push_back(frame_duration);

    int percentage = (frame_number * 100) / total_frames_;
    int remaining = total_frames_ - frame_number;

    // Running average of frame durations for ETA
    double avg_duration = std::accumulate(frame_durations_.begin(), frame_durations_.end(), 0.0)
                          / static_cast<double>(frame_durations_.size());
    double eta = avg_duration * remaining;

    // Progress bar: 20 chars wide
    int bar_width = 20;
    int filled = (frame_number * bar_width) / total_frames_;
    int empty = bar_width - filled;

    output_ << "Frame " << frame_number << "/" << total_frames_
            << " (" << percentage << "%) [";
    for (int i = 0; i < filled; ++i) output_ << '=';
    if (filled < bar_width) {
        output_ << '>';
        for (int i = 1; i < empty; ++i) output_ << ' ';
    }
    output_ << "]";

    if (remaining > 0) {
        output_ << " ETA: " << std::fixed << std::setprecision(1) << eta << "s";
    }

    output_ << "\n";
}

void StreamProgressReporter::finish() {
    auto now = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(now - start_time_).count();
    output_ << "Done in " << std::fixed << std::setprecision(1) << elapsed << "s\n";
}

} // namespace nwave
