#pragma once

namespace logicsim {
    class Simulator;
    // writeReport=false levelises without touching the filesystem. The LEV
    // command names its report file in the command arguments, so anything that
    // levelises internally has to pass false or it writes its report over
    // whatever path happens to be sitting in there.
    int lev_impl(Simulator &simulator, bool writeReport = true);
}
