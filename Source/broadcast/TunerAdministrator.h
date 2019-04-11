#ifndef TUNER_ADMINISTRATOR_H
#define TUNER_ADMINISTRATOR_H

#include "Definitions.h"
#include "MPEGTable.h"

namespace WPEFramework {

namespace Broadcast {

    // This class is the linking pin (Singleton) between the Tuner Implementation and the outside world.
    // However the definition of this class should not be shared with the outside world as this class 
    // should not be directly accessed by anyone in the outside world
    // The purpose of this class is to allow SI/PSI parsing on tuners while thay are not yet tuned, or
    // if they are tuned, to collect (P)SI information on that transport stream and so it looks to the
    // outside world that if one tunes automagically the P(SI) get populated.
    // So: DO *NOT* USE THIS CLASS OUTSIDE THE BROADCAST LIBRARY, THAT IS WHY IT IS IN ANY OF THE 
    //     INCLUDES OF ANY OTHER HEADER FILES AND ONLY USED BY THE TUNER IMPLEMENTATIONS !!!!
    class TunerAdministrator {
    public:
        struct ICallback {
            virtual ~ICallback() {}

            virtual void StateChange(ITuner* tuner) = 0;
        };

    private:
        TunerAdministrator(const TunerAdministrator&) = delete;
        TunerAdministrator& operator=(const TunerAdministrator&) = delete;

        class Sink : public ICallback {
        private:
            Sink() = delete;
            Sink(const Sink&) = delete;
            Sink& operator=(const Sink&) = delete;

        public:
            Sink(TunerAdministrator& parent)
                : _parent(parent)
            {
            }
            virtual ~Sink()
            {
            }

        public:
            virtual void StateChange(ITuner* tuner) override
            {
                _parent.StateChange(tuner);
            }

        private:
            TunerAdministrator& _parent;
        };

        TunerAdministrator()
            : _adminLock()
            , _sink(*this)
            , _observers()
            , _tuners()
        {
        }

        typedef std::list<ITuner::INotification*> Observers;
        typedef std::list<ITuner*> Tuners;

    public:
        static TunerAdministrator& Instance();
        virtual ~TunerAdministrator() {}

    public:
        void Register(ITuner::INotification* observer)
        {
            _adminLock.Lock();

            ASSERT(std::find(_observers.begin(), _observers.end(), observer) == _observers.end());

            _observers.push_back(observer);

            Tuners::iterator index(_tuners.begin());
            while (index != _tuners.end()) {
                observer->Activated(*index);
                observer->StateChange(*index);
                index++;
            }

            _adminLock.Unlock();
        }
        void Unregister(ITuner::INotification* observer)
        {
            _adminLock.Lock();

            Observers::iterator index(std::find(_observers.begin(), _observers.end(), observer));

            if (index != _observers.end()) {
                _observers.erase(index);
            }

            _adminLock.Unlock();
        }
        ICallback* Announce(ITuner* tuner)
        {
            _adminLock.Lock();

            ASSERT(std::find(_tuners.begin(), _tuners.end(), tuner) == _tuners.end());

            Observers::iterator index(_observers.begin());

            while (index != _observers.end()) {
                (*index)->Activated(tuner);
                index++;
            }

            _tuners.push_back(tuner);

            _adminLock.Unlock();

            return (&_sink);
        }
        void Revoke(ITuner* tuner)
        {
            _adminLock.Lock();

            Tuners::iterator entry(std::find(_tuners.begin(), _tuners.end(), tuner));

            ASSERT(entry != _tuners.end());

            if (entry != _tuners.end()) {

                Observers::iterator index(_observers.begin());

                _tuners.erase(entry);

                while (index != _observers.end()) {
                    (*index)->Deactivated(tuner);
                    index++;
                }
            }

            _adminLock.Unlock();
        }

    private:
        void StateChange(ITuner* tuner)
        {
            // Before notifying the others, lets notify the real user of the ITuner interface
            tuner->StateChange();

            _adminLock.Lock();

            ASSERT(std::find(_tuners.begin(), _tuners.end(), tuner) != _tuners.end());

            Observers::iterator index(_observers.begin());

            while (index != _observers.end()) {
                (*index)->StateChange(tuner);
                index++;
            }

            _adminLock.Unlock();
        }

    private:
        Core::CriticalSection _adminLock;
        Sink _sink;
        Observers _observers;
        Tuners _tuners;
    };

} // namespace Broadcast
} // namespace WPEFramework

#endif // TUNER_ADMINISTRATOR_H
