#include "../includes/Command.hpp"

#include <cstring>

namespace logicsim {
    using Handler = std::function<void()>;
    Command::Command() {
        name[0] = '\0';
        handler = nullptr;
        state = State::EXEC;
    }
    void Command::setName(const char* name) {
        strncpy(this->name, name, MAXNAME);
    }
    const char* Command::getName() {
        return name;
    }
    void Command::setHandler(Handler handler) {
        this->handler = handler;
    }
    Handler Command::getHandler() {
        return handler;
    }
    void Command::setState(State state) {
        this->state = state;
    }
    State Command::getState() {
        return state;
    }
    void Command::registerCommand(const char* name, Handler handler, State state) {
        setName(name);
        setHandler(handler);
        setState(state);
    }
    void Command::invoke() {
        if (handler) {
            handler();
        }
    }

}
