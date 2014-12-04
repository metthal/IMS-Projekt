#include <ctime>
#include <simlib.h>
#include "machine.h"

#define TRACE_ENABLE
#ifdef TRACE_ENABLE
    #define TRACE(fmt, ...)     Print("%.3f: ", Time); Print(fmt, ##__VA_ARGS__); Print("\n")
#else
    #define TRACE(fmt, ...)     (void)fmt
#endif

#define SMT_LINES               1
#define BOARDS_PER_REQUEST      1

Machine screenPrinters[SMT_LINES];
Machine pnpMachines[SMT_LINES];
Machine aoiMachines[SMT_LINES];
Queue smtQueue;

class Board : public Process
{
public:
    Board() : Process(), _id(IDFactory++), _linkId(~0) {}

    void Behavior()
    {
TRACE("Nova doska (%u)", _id);
        // Najdi volny screen printer
        int i;
        for (i = 0; i < SMT_LINES; ++i)
        {
            if (!screenPrinters[i].Busy())
            {
                Seize(screenPrinters[i]);
TRACE("Doska (%u) zabrala screen printer %d", _id, i);
                break;
            }
        }
        _linkId = i;

        // Ak nebol ziadny volny, zarad dosku do fronty
        if (_linkId == SMT_LINES)
        {
TRACE("Doska (%u) sa zaradila do fronty lebo nie je nic volne", _id);
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
TRACE("Doska (%u) zabrala screen printer %d", _id, _linkId);
        }

        Wait(5); // TODO: samotny screen printing

TRACE("Doska (%u) opustila screen printer %d", _id, _linkId);
        Release(screenPrinters[_linkId]);

        Seize(pnpMachines[_linkId]); // vstup do odpovedajuceho P&P pristroja
TRACE("Doska (%u) vstupila do P&P %d", _id, _linkId);
        Wait(3.5); // umiestnovanie SMD
TRACE("Doska (%u) opustila P&P %d", _id, _linkId);
        Release(pnpMachines[_linkId]); // uvolnenie P&P pristroja

TRACE("Doska (%u) vstupila do reflow oven", _id);
        Wait(3); // Reflow Oven
TRACE("Doska (%u) opustila reflow oven", _id);

        if (AOI() == false) // pokial doska nepresla cez AOI
        {
            Priority = 1;   // zvys prioritu
            do
            {
                Wait(4);    // oprava dosky
            }
            while (AOI() == false); // pokial doska obsahuje chyby, opravujeme ju
            Priority = 0;   // po prejdeni AOI zniz prioritu
        }
    }

    bool AOI()
    {
        bool passed = true;

        Seize(aoiMachines[_linkId]);
TRACE("Doska (%u) vstupila do AOI %d", _id, _linkId);
        Wait(2); // AOI proces

        if (Uniform(0, 100) < 5) // 5% pravdepodobnost chyby
        {
TRACE("Doska (%u) nepresla AOI %d", _id, _linkId);
            passed = false;
        }

TRACE("Doska (%u) opustila AOI %d", _id, _linkId);
        Release(aoiMachines[_linkId]);
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
        for (int i = 0; i < BOARDS_PER_REQUEST; ++i) // pocet dosiek v jednej poziadavke na vyrobu
            (new Board)->Activate();
        Activate(Time + 1); // TODO: fixnut tuto konstantu
    }
};

unsigned int Board::IDFactory = 0;
int main(int argc, char* argv[])
{
    // Inicializuje 18 screen printerov
    for (int i = 0; i < SMT_LINES; ++i)
    {
        screenPrinters[i].SetNameWithNum("Solder Paste Screen Printer", i);
        screenPrinters[i].SetQueue(&smtQueue);
        pnpMachines[i].SetNameWithNum("Pick & Place Machine", i);
        aoiMachines[i].SetNameWithNum("AOI Machine", i);
    }

    RandomSeed(time(NULL));
    Init(0, 99);
    (new Generator)->Activate();
    Run();

/*    for (int i = 0; i < SMT_LINES; ++i)
    {
        screenPrinters[i].Output();
        pnpMachines[i].Output();
    }*/
}
