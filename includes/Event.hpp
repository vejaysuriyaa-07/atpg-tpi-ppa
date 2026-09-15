#include <map>
#include <utility>
#include <vector>

#include "Node.hpp"
namespace logicsim {
    class Event {
        private:
            Node* targetNode;
            LogicValue newValue;
            int eventTime;
        public:
            Event(Node* node, LogicValue value, int time) 
                : targetNode(node), newValue(value), eventTime(time) {}
            Node* getTargetNode() const { return targetNode; }
            LogicValue getNewValue() const { return newValue; }
            int getEventTime() const { return eventTime; }
    };

    class EventTable {
        private:
            std::map<int, std::vector<Event>> eventsByTime;

            void discardOlderThan(int timeLimit) {
                auto it = eventsByTime.begin();
                while (it != eventsByTime.end() && it->first < timeLimit) {
                    it = eventsByTime.erase(it);
                }
            }
        public:
            bool empty() const {
                return eventsByTime.empty();
            }

            bool nextEventTime(int& time) const {
                if (eventsByTime.empty()) {
                    return false;
                }
                time = eventsByTime.begin()->first;
                return true;
            }

            void insert(const Event& event) {
                eventsByTime[event.getEventTime()].push_back(event);
            }

            void insert(Node* node, LogicValue value, int time) {
                insert(Event(node, value, time));
            }

            std::vector<Event> extract(int time) {
                discardOlderThan(time);
                auto pos = eventsByTime.find(time);
                if (pos == eventsByTime.end()) {
                    return {};
                }
                auto bucket = std::move(pos->second);
                eventsByTime.erase(pos);
                return bucket;
            }

            void prune(int currentTime) {
                discardOlderThan(currentTime);
            }

            void clear() {
                eventsByTime.clear();
            }

            // void printTable() const {
            //     for (std::map<int, std::vector<Event>>::const_iterator it = eventsByTime.begin();
            //          it != eventsByTime.end();
            //          ++it) {
            //         const int time = it->first;
            //         const std::vector<Event>& events = it->second;
            //         for (std::vector<Event>::const_iterator evtIt = events.begin(); evtIt != events.end(); ++evtIt) {
            //             printf("Time %d: Node %u -> %d\n",
            //                    time,
            //                    evtIt->getTargetNode()->getNum(),
            //                    evtIt->getNewValue());
            //         }
            //     }
            // }
    };
}
