# Legacy Samsung RIL contract checks

Run on a Linux host with a C++ compiler and libresolv:

```sh
c++ -Wall -Wextra -Werror LegacyDataProfileTest.cpp -o /tmp/legacy-profile-test
/tmp/legacy-profile-test
c++ -Wall -Wextra -Werror LegacySimAuthTest.cpp -lresolv -o /tmp/legacy-auth-test
/tmp/legacy-auth-test
```

The checks cover Android non-persistent IMS profile IDs while preserving other
APN types, USIM/GSM request encoding, malformed input and successful/AUTS reply
encoding. They exercise the same inline adapters used by ril_service.cpp.
They do not access a modem or use real SIM authentication data.
