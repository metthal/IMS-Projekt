#include <ctime>
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

#define BOARDS_PER_REQUEST  54795

// SMT
#define SMT_LINES 18
Queue smtQueue;
Machine screenPrinters[SMT_LINES];
Machine pnpMachines[SMT_LINES];
Machine aoiMachines[SMT_LINES];

// DIP
#define DIP_LINES 10
Line dipLines[DIP_LINES];
unsigned int currentDipLine = 0;

// Testing
#define TESTING_LINES 9
Line testingLines[TESTING_LINES];
unsigned int currentTestingLine = 0;

// Packing
#define PACKING_LINES 10
Line packingLines[PACKING_LINES];
unsigned int currentPackingLine = 0;

unsigned int boardsRequested = 0;
unsigned int boardsMade = 0;
unsigned int boardsSMT = 0;

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
        int i;
        for (i = 0; i < SMT_LINES; ++i)
        {
            if (!screenPrinters[i].Busy())
            {
                Seize(screenPrinters[i]);
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
                if (screenPrinters[i].in == this)
                {
                    _linkId = i;
                    break;
                }
            }
        }

        TRACE("Doska (%u) zabrala screen printer %d", _id, _linkId);
        Wait(Uniform(10, 20)); // samotny screen printing

        TRACE("Doska (%u) opustila screen printer %d", _id, _linkId);
        Release(screenPrinters[_linkId]);

        Seize(pnpMachines[_linkId]); // vstup do odpovedajuceho P&P pristroja
        TRACE("Doska (%u) vstupila do P&P %d", _id, _linkId);
        Wait(Uniform(1 * MINUTES, 1.5 * MINUTES)); // umiestnovanie SMD
        TRACE("Doska (%u) opustila P&P %d", _id, _linkId);
        Release(pnpMachines[_linkId]); // uvolnenie P&P pristroja

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
        Enter(dipLines[_linkId], 1);
        TRACE("Doska (%u) vstupila na DIP linku %d", _id, _linkId);

        // prechod cez DIP link + wave oven
        Wait(2 * MINUTES);

        // opustame pas
        TRACE("Doska (%u) opustila DIP linku %d", _id, _linkId);
        Leave(dipLines[_linkId], 1);

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
        Enter(packingLines[_linkId], 1);
        TRACE("Doska (%u) vstupila na baliacu linku %d", _id, _linkId);

        // balenie
        Wait(3 * MINUTES);

        // opustame pas
        TRACE("Doska (%u) opustila baliacu linku %d", _id, _linkId);
        Leave(packingLines[_linkId], 1);

        boardsMade++;
    }

    bool AOI()
    {
        bool passed = true;

        Seize(aoiMachines[_linkId]);
        TRACE("Doska (%u) vstupila do AOI %d", _id, _linkId);
        Wait(Uniform(6, 12)); // AOI proces

        if (Uniform(0, 100) < 1) // 1% pravdepodobnost chyby
        {
            TRACE("Doska (%u) nepresla AOI %d", _id, _linkId);
            passed = false;
        }

        TRACE("Doska (%u) opustila AOI %d", _id, _linkId);
        Release(aoiMachines[_linkId]);
        return passed;
    }

    bool Testing()
    {
        bool passed = true;

        _linkId = currentTestingLine++;
        if (currentTestingLine == TESTING_LINES)
            currentTestingLine = 0;

        Enter(testingLines[_linkId], 1);
        TRACE("Doska (%u) zabrala Testing stroj %d", _id, _linkId);
        Wait(Uniform(1 * MINUTES, 2 * MINUTES)); // Testing proces

        if (Uniform(0, 100) < 1) // 1% pravdepodobnost chyby
        {
            TRACE("Doska (%u) nepresla cez Testing machine %d", _id, _linkId);
            passed = false;
        }

        TRACE("Doska (%u) opustila Testing machine %d", _id, _linkId);
        Leave(testingLines[_linkId], 1);
        return passed;
    }

private:
    unsigned int _id, _linkId;

    static unsigned int IDFactory;
};

// Generator poziadavkov na vyrobu
int day = -1;
class Generator : public Event
{
public:
    void Behavior()
    {
        for (int i = 0; i < BOARDS_PER_REQUEST; ++i)
            (new Board)->Activate();
        // pre vyrobenie 21 milionov dosiek za rok je potrebe poziadavok na vyrobu dosky kazdych ~1.5 sekundy
        if ((int)(Time / 86400) != day)
        {
            Print("Day: %d\n", (int)(Time / 86400) + 1);
            day++;
        }
        Activate(Time + 86400);
    }
};

unsigned int Board::IDFactory = 0;
int main(int argc, char* argv[])
{
    // Inicializacia pristrojov v SMT linke
    for (int i = 0; i < SMT_LINES; ++i)
    {
        screenPrinters[i].SetNameWithNum("Solder Paste Screen Printer", i);
        screenPrinters[i].SetQueue(&smtQueue);
        pnpMachines[i].SetNameWithNum("Pick & Place Machine", i);
        aoiMachines[i].SetNameWithNum("AOI Machine", i);
    }

    // Inicializacia DIP linky
    for (int i = 0; i < DIP_LINES; ++i)
    {
        dipLines[i].SetNameWithNum("DIP Line", i);
        dipLines[i].SetCapacity(50);
    }

    // Inicializacia Testing linky
    for (int i = 0; i < TESTING_LINES; ++i)
    {
        testingLines[i].SetNameWithNum("Testing Machine", i);
        testingLines[i].SetCapacity(50);
    }

    // Inicializacia Packing linky
    for (int i = 0; i < PACKING_LINES; ++i)
    {
        packingLines[i].SetNameWithNum("Packing Line", i);
        packingLines[i].SetCapacity(50);
    }

    RandomSeed(time(NULL));
//    Init(0, 2591999);
//    Init(0, 604799);
    Init(0, 1 * 86399);
    (new Generator)->Activate();
    Run();

    for (int i = 0; i < SMT_LINES; ++i)
    {
        screenPrinters[i].Output();
        pnpMachines[i].Output();
        aoiMachines[i].Output();
    }

    for (int i = 0; i < DIP_LINES; ++i)
    {
        dipLines[i].Output();
    }

    for (int i = 0; i < TESTING_LINES; ++i)
    {
        testingLines[i].Output();
    }

    for (int i = 0; i < PACKING_LINES; ++i)
    {
        packingLines[i].Output();
    }

    smtQueue.Output();

    Print("Boards Requested: %u\n", boardsRequested);
    Print("Boards SMT: %u\n", boardsSMT);
    Print("Boards Made: %u\n", boardsMade);
}
