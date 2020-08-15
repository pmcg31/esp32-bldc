#define P0_LIN 25
#define P0_HIN 26
#define P1_LIN 32
#define P1_HIN 33
#define P2_LIN 14
#define P2_HIN 12

#define P_FLOAT 0
#define P_HI 1
#define P_LO 2

#define INDEX_CHANGE_MS 1

HardwareSerial &Debug = Serial;

const char *compileDate = __DATE__;
const char *compileTime = __TIME__;

// const int commPattern[12][3] = {
//     {P_HI, P_LO, P_FLOAT},
//     {P_LO, P_LO, P_FLOAT},
//     {P_HI, P_FLOAT, P_LO},
//     {P_LO, P_FLOAT, P_LO},
//     {P_FLOAT, P_HI, P_LO},
//     {P_FLOAT, P_LO, P_LO},
//     {P_LO, P_HI, P_FLOAT},
//     {P_LO, P_LO, P_FLOAT},
//     {P_LO, P_FLOAT, P_HI},
//     {P_LO, P_FLOAT, P_LO},
//     {P_FLOAT, P_LO, P_HI},
//     {P_FLOAT, P_LO, P_LO}};
const int commPattern[12][3] = {
    {P_HI, P_LO, P_FLOAT},
    {P_FLOAT, P_LO, P_FLOAT},
    {P_HI, P_FLOAT, P_LO},
    {P_FLOAT, P_FLOAT, P_LO},
    {P_FLOAT, P_HI, P_LO},
    {P_FLOAT, P_FLOAT, P_LO},
    {P_LO, P_HI, P_FLOAT},
    {P_LO, P_FLOAT, P_FLOAT},
    {P_LO, P_FLOAT, P_HI},
    {P_LO, P_FLOAT, P_FLOAT},
    {P_FLOAT, P_LO, P_HI},
    {P_FLOAT, P_LO, P_FLOAT}};

int idx = 0;

int testLIN = P0_LIN;
int testHIN = P0_HIN;

boolean ledOn = false;

volatile int interruptCounter;
int totalInterruptCounter;
int totalCycleCounter;

int cycleResetSlow = 1000;
int cycleResetFast = 150;
int cycleReset;
int accelerateCycles = 30000;
int steadyCycles = 100000;
bool accel = true;
bool decel = false;
int accelCounter;
int accelCountPerCycleReset;

hw_timer_t *timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

void IRAM_ATTR onTimer();
void setPhaseState(int lin, int hin, int state);
void setCommIndex(int commIdx);

void setup()
{
    Debug.begin(115200);
    Debug.println("Booting");
    Debug.print("Compile date: ");
    Debug.println(compileDate);
    Debug.print("Compile time: ");
    Debug.println(compileTime);

    interruptCounter = 0;
    totalInterruptCounter = 0;
    totalCycleCounter = 0;
    accelCounter = 0;

    accelCountPerCycleReset = accelerateCycles / (cycleResetSlow - cycleResetFast);
    Debug.printf("Accel count: %d\n", accelCountPerCycleReset);

    cycleReset = cycleResetSlow;

    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, ledOn ? HIGH : LOW);

    pinMode(P0_LIN, OUTPUT);
    pinMode(P0_HIN, OUTPUT);
    pinMode(P1_LIN, OUTPUT);
    pinMode(P1_HIN, OUTPUT);
    pinMode(P2_LIN, OUTPUT);
    pinMode(P2_HIN, OUTPUT);

    digitalWrite(P0_LIN, HIGH);
    digitalWrite(P0_HIN, LOW);
    digitalWrite(P1_LIN, HIGH);
    digitalWrite(P1_HIN, LOW);
    digitalWrite(P2_LIN, HIGH);
    digitalWrite(P2_HIN, LOW);

    timer = timerBegin(0, 80, true);
    timerAttachInterrupt(timer, &onTimer, true);
    timerAlarmWrite(timer, 10, true);
    timerAlarmEnable(timer);

    setCommIndex(idx);
}

void loop()
{
    if (interruptCounter > 0)
    {
        portENTER_CRITICAL(&timerMux);
        interruptCounter--;
        portEXIT_CRITICAL(&timerMux);

        totalInterruptCounter++;

        if (totalCycleCounter >= (int)cycleReset)
        {
            idx += 2;
            if (idx > 10)
            {
                idx = 0;
            }

            totalCycleCounter = 0;

            ledOn = !ledOn;
            digitalWrite(LED_BUILTIN, ledOn ? HIGH : LOW);
        }

        boolean backOff = (totalInterruptCounter == 2);
        boolean startOver = (totalInterruptCounter == 4);
        if (backOff)
        {
            setCommIndex(idx + 1);
        }

        if (startOver)
        {
            setCommIndex(idx);

            if (startOver)
            {
                totalInterruptCounter = 0;
                totalCycleCounter++;
                accelCounter++;

                if (accel)
                {
                    if ((accelCounter % accelCountPerCycleReset) == 0)
                    {
                        cycleReset--;
                    }

                    if (accelCounter >= accelerateCycles)
                    {
                        accel = false;
                        decel = false;
                        accelCounter = 0;
                        Debug.printf("steady %d\n", cycleReset);
                    }
                }
                else if (decel)
                {
                    if ((accelCounter % accelCountPerCycleReset) == 0)
                    {
                        cycleReset++;
                    }

                    if (accelCounter >= accelerateCycles)
                    {
                        decel = false;
                        accel = true;
                        accelCounter = 0;
                        Debug.println("accel");
                    }
                }
                else
                {
                    if (accelCounter >= steadyCycles)
                    {
                        decel = true;
                        accel = false;
                        accelCounter = 0;
                        Debug.println("decel");
                    }
                }
            }
        }
    }
}

void IRAM_ATTR onTimer()
{
    portENTER_CRITICAL_ISR(&timerMux);
    interruptCounter++;
    portEXIT_CRITICAL_ISR(&timerMux);
}

void setPhaseState(int lin, int hin, int state)
{
    switch (state)
    {
    case P_FLOAT:
        // Float
        // Debug.println(lin + " Float");
        digitalWrite(lin, LOW);
        digitalWrite(hin, LOW);
        break;
    case P_HI:
        // HI
        // Debug.println(lin + " HI");
        digitalWrite(lin, LOW);
        digitalWrite(hin, HIGH);
        break;
    case P_LO:
        // LO
        // Debug.println(lin + " LO");
        digitalWrite(hin, LOW);
        digitalWrite(lin, HIGH);
        break;
    }
}

void setCommIndex(int commIdx)
{
    setPhaseState(P0_LIN, P0_HIN, commPattern[commIdx][0]);
    setPhaseState(P1_LIN, P1_HIN, commPattern[commIdx][1]);
    setPhaseState(P2_LIN, P2_HIN, commPattern[commIdx][2]);
}
