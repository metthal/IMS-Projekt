#include <ctime>
#include <cstring>
#include <vector>
#include <simlib.h>
#include "machine.h"
#include "line.h"

//#define TRACE_ENABLE
#ifdef TRACE_ENABLE
    #define TRACE(fmt, ...)     Print("%.3f: ", Time); Print(fmt, ##__VA_ARGS__); Print("\n")
#else
    #define TRACE(fmt, ...)     (void)fmt
#endif

#define MINUTES         60.0
#define HOURS           3600.0

unsigned int REQUEST_PER_DAY = 19179;

// SMT
unsigned int SMT_LINES = 18;
Queue smtQueue("SMT Fronta");
std::vector<Machine*> screenPrinters;
std::vector<Machine*> pnpMachines;
std::vector<Machine*> aoiMachines;
double minPnpTime = 1 * MINUTES;
double maxPnpTime = 1.5 * MINUTES;
double aoiErrorRate = 1.0;

// DIP
unsigned int DIP_LINES = 10;
std::vector<Line*> dipLines;
unsigned int currentDipLine = 0;

// Testing
unsigned int TESTING_LINES = 9;
std::vector<Line*> testingLines;
unsigned int currentTestingLine = 0;
double testingErrorRate = 1.0;

// Packing
unsigned int PACKING_LINES = 10;
std::vector<Line*> packingLines;
unsigned int currentPackingLine = 0;

unsigned int boardsRequested = 0;
unsigned int boardsMade = 0;
unsigned int boardsSMT = 0;

Histogram doskaVoVyrobeD("Doska vo vyrobe - 2h/24h", 0, 2 * HOURS, 12);
Histogram doskaVoVyrobeH("Doska vo vyrobe - 5m/1h", 0, 5 * MINUTES, 12);

class Board : public Process
{
public:
    Board() : Process(), _id(IDFactory++), _linkId(~0) {}

    void Behavior()
    {
        boardsRequested++;
        TRACE("Nova doska (%u)", _id);
        // SMT
        // Najdi volny screen printer
        unsigned int i;
        for (i = 0; i < SMT_LINES; ++i)
        {
            if (!screenPrinters[i]->Busy())
            {
                Seize(*screenPrinters[i]);
                break;
            }
        }
        _linkId = i;

        // Ak nebol ziadny volny, zarad dosku do fronty
        if (_linkId == SMT_LINES)
        {
            TRACE("Doska (%u) sa zaradila do SMT fronty lebo nie je nic volne", _id);
            Into(smtQueue);
            Passivate();

            // ak sme predtym nenasli volny screen printer, musime zistit, ktory nam bol neskor prideleny
            for (i = 0; i < SMT_LINES; ++i)
            {
                if (screenPrinters[i]->in == this)
                {
                    _linkId = i;
                    break;
                }
            }
        }

        double zaciatokVyroby = Time;
        TRACE("Doska (%u) zabrala screen printer %d", _id, _linkId);
        Wait(Uniform(30, 45)); // samotny screen printing

        TRACE("Doska (%u) opustila screen printer %d", _id, _linkId);
        Release(*screenPrinters[_linkId]);

        Seize(*pnpMachines[_linkId]);
        TRACE("Doska (%u) vstupila do P&P %d:%d (%d)", _id, _linkId, i, _linkId * PNP_PER_LINE + i);

        Wait(Uniform(minPnpTime, maxPnpTime)); // umiestnovanie SMD

        TRACE("Doska (%u) opustila P&P %d", _id, _linkId);
        Release(*pnpMachines[_linkId]); // uvolnenie P&P pristroja

        TRACE("Doska (%u) vstupila do reflow oven", _id);
        Wait(5 * MINUTES); // Reflow Oven
        TRACE("Doska (%u) opustila reflow oven", _id);

        if (AOI() == false) // pokial doska nepresla cez AOI
        {
            Priority = 1;   // zvys prioritu
            do
            {
                Wait(Exponential(6 * MINUTES)); // oprava dosky
            }
            while (AOI() == false); // pokial doska obsahuje chyby, opravujeme ju
            Priority = 0;   // po prejdeni AOI zniz prioritu
        }

        boardsSMT++;

        // DIP
        // zvoli sa jedna DIP linka a dalsia doska pojde do dalsej DIP linky, a tak dokola
        _linkId = currentDipLine++;
        if (currentDipLine == DIP_LINES)
            currentDipLine = 0;

        // vstup
        Enter(*dipLines[_linkId], 1);
        TRACE("Doska (%u) vstupila na DIP linku %d", _id, _linkId);

        // prechod cez DIP link + wave oven
        Wait(2 * MINUTES);

        // opustame pas
        TRACE("Doska (%u) opustila DIP linku %d", _id, _linkId);
        Leave(*dipLines[_linkId], 1);

        // Testing
        if (Testing() == false)
        {
            Priority = 1; // zvysena priorita pre opravene dosky
            do
            {
                Wait(Exponential(3 * MINUTES)); // oprava dosky
            }
            while (Testing() == false);
            Priority = 0; // opat znizime prioritu
        }

        // Packing
        _linkId = currentPackingLine++;
        if (currentPackingLine == PACKING_LINES)
            currentPackingLine = 0;

        // vstup
        Enter(*packingLines[_linkId], 1);
        TRACE("Doska (%u) vstupila na baliacu linku %d", _id, _linkId);

        // balenie
        Wait(Uniform(40, 60));

        // opustame pas
        TRACE("Doska (%u) opustila baliacu linku %d", _id, _linkId);
        Leave(*packingLines[_linkId], 1);

        boardsMade++;

        doskaVoVyrobeD(Time - zaciatokVyroby);
        doskaVoVyrobeH(Time - zaciatokVyroby);
    }

    bool AOI()
    {
        bool passed = true;

        Seize(*aoiMachines[_linkId]);
        TRACE("Doska (%u) vstupila do AOI %d", _id, _linkId);
        Wait(Uniform(10, 14)); // AOI proces

        if (Uniform(0, 100) < aoiErrorRate) // 1% pravdepodobnost chyby
        {
            TRACE("Doska (%u) nepresla AOI %d", _id, _linkId);
            passed = false;
        }

        TRACE("Doska (%u) opustila AOI %d", _id, _linkId);
        Release(*aoiMachines[_linkId]);
        return passed;
    }

    bool Testing()
    {
        bool passed = true;

        _linkId = currentTestingLine++;
        if (currentTestingLine == TESTING_LINES)
            currentTestingLine = 0;

        Enter(*testingLines[_linkId], 1);
        TRACE("Doska (%u) zabrala Testing stroj %d", _id, _linkId);
        Wait(Exponential(2 * MINUTES)); // Testing proces

        if (Uniform(0, 100) < testingErrorRate) // 1% pravdepodobnost chyby
        {
            TRACE("Doska (%u) nepresla cez Testing machine %d", _id, _linkId);
            passed = false;
        }

        TRACE("Doska (%u) opustila Testing machine %d", _id, _linkId);
        Leave(*testingLines[_linkId], 1);
        return passed;
    }

private:
    unsigned int _id, _linkId;

    static unsigned int IDFactory;
};

// Generator poziadavkov na vyrobu
class Generator : public Event
{
public:
    void Behavior()
    {
        for (unsigned int i = 0; i < REQUEST_PER_DAY; ++i)
            (new Board)->Activate();

        Activate(Time + 24 * HOURS); // poziadavok na vyrobu kazdy den
    }
};

unsigned int Board::IDFactory = 0;
int main(int argc, char* argv[])
{
    // velmi primitivne spracovanie parametrov len pre testovacie ucely
    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "--smt") == 0)
        {
            SMT_LINES = std::atoi(argv[i + 1]);
            i += 1;
            continue;
        }
        else if (strcmp(argv[i], "--dip") == 0)
        {
            DIP_LINES = std::atoi(argv[i + 1]);
            i += 1;
            continue;
        }
        else if (strcmp(argv[i], "--tst") == 0)
        {
            TESTING_LINES = std::atoi(argv[i + 1]);
            i += 1;
            continue;
        }
        else if (strcmp(argv[i], "--pkg") == 0)
        {
            PACKING_LINES = std::atoi(argv[i + 1]);
            i += 1;
            continue;
        }
        else if (strcmp(argv[i], "--req") == 0)
        {
            REQUEST_PER_DAY = std::atoi(argv[i + 1]);
            i += 1;
            continue;
        }
        else if (strcmp(argv[i], "--pnp") == 0)
        {
            minPnpTime = std::atof(argv[i + 1]);
            maxPnpTime = std::atof(argv[i + 2]);
            i += 2;
            continue;
        }
        else if (strcmp(argv[i], "--out") == 0)
        {
            SetOutput(argv[i + 1]);
            i += 1;
            continue;
        }
        else if (strcmp(argv[i], "--err") == 0)
        {
            aoiErrorRate = std::atof(argv[i + 1]);
            testingErrorRate = std::atof(argv[i + 2]);
            i += 2;
            continue;
        }
    }

    // Inicializacia pristrojov v SMT linke
    screenPrinters.resize(SMT_LINES);
    pnpMachines.resize(SMT_LINES);
    aoiMachines.resize(SMT_LINES);
    for (unsigned int i = 0; i < SMT_LINES; ++i)
    {
        screenPrinters[i] = new Machine;
        screenPrinters[i]->SetNameWithNum("Solder Paste Screen Printer", i);
        screenPrinters[i]->SetQueue(&smtQueue);

        pnpMachines[i] = new Machine;
        pnpMachines[i]->SetNameWithNum("Pick & Place Machine", i);

        aoiMachines[i] = new Machine;
        aoiMachines[i]->SetNameWithNum("AOI Machine", i);
    }

    // Inicializacia DIP linky
    dipLines.resize(DIP_LINES);
    for (unsigned int i = 0; i < DIP_LINES; ++i)
    {
        dipLines[i] = new Line;
        dipLines[i]->SetNameWithNum("DIP Line", i);
        dipLines[i]->SetCapacity(50);
    }

    // Inicializacia Testing linky
    testingLines.resize(TESTING_LINES);
    for (unsigned int i = 0; i < TESTING_LINES; ++i)
    {
        testingLines[i] = new Line;
        testingLines[i]->SetNameWithNum("Testing Line", i);
        testingLines[i]->SetCapacity(15);
    }

    // Inicializacia Packing linky
    packingLines.resize(PACKING_LINES);
    for (unsigned int i = 0; i < PACKING_LINES; ++i)
    {
        packingLines[i] = new Line;
        packingLines[i]->SetNameWithNum("Packing Line", i);
        packingLines[i]->SetCapacity(30);
    }

    Init(0, 24 * HOURS - 1); // 1 den
    RandomSeed(time(NULL));
    (new Generator)->Activate();
    Run();

    for (unsigned int i = 0; i < SMT_LINES; ++i)
    {
        screenPrinters[i]->Output();
        pnpMachines[i]->Output();
        aoiMachines[i]->Output();
    }

    for (unsigned int i = 0; i < DIP_LINES; ++i)
    {
        dipLines[i]->Output();
    }

    for (unsigned int i = 0; i < TESTING_LINES; ++i)
    {
        testingLines[i]->Output();
    }

    for (unsigned int i = 0; i < PACKING_LINES; ++i)
    {
        packingLines[i]->Output();
    }

    smtQueue.Output();

    Print("Boards Requested: %u\n", boardsRequested);
    Print("Boards SMT: %u\n", boardsSMT);
    Print("Boards Made: %u\n", boardsMade);

    // Upraceme si pamat
    for (unsigned int i = 0; i < SMT_LINES; ++i)
    {
        delete screenPrinters[i];
        delete pnpMachines[i];
        delete aoiMachines[i];
    }

    for (unsigned int i = 0; i < DIP_LINES; ++i)
    {
        delete dipLines[i];
    }

    for (unsigned int i = 0; i < TESTING_LINES; ++i)
    {
        delete testingLines[i];
    }

    for (unsigned int i = 0; i < PACKING_LINES; ++i)
    {
        delete packingLines[i];
    }

    doskaVoVyrobeD.Output();
    doskaVoVyrobeH.Output();
}
