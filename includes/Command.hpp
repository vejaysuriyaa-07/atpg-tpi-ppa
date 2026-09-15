#pragma once

#include "types.hpp" 
#include <functional>
namespace logicsim {
    class Command {
        public:
            Command();
            using Handler = std::function<void()>;
            void setName(const char* name);
            const char* getName();
            void setHandler(Handler handler);
            Handler getHandler();
            void setState(State state);
            State getState();
            void registerCommand(const char* name, Handler handler, State state);
            void invoke();
        private:
            char name[MAXNAME]; /* command syntax */
            Handler handler;    /* function pointer of the commands */
            State state; /* execution state sequence */
    };
}
