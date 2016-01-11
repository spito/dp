
\chapter{Struktura archivu a demonstrační program}\label{chap:appendix}

V\ souboru `archive.zip` se\ nachází zdrojové soubory nově implementovaného komunikačního rozhraní. Spolu s\ nimi tam jsou skripty, které slouží k\ vygenerování testů a\ jejich spuštění. Pro překlad je\ potřeba překladač s\ plnou podporou standardu C++11 a\ pro generování a\ spouštění testů je\ zapotřebí interpret jazyka Python 3. Po\ nakonfigurování projektu programem `cmake` je\ možné projekt přeložit.

Ve\ složce `measurement/execute` se\ nachází dva skripty. První z\ nich, `generate.py`, slouží k\ vygenerování sady testů. Akceptuje jeden až\ několik parametrů při spuštění, což jsou názvy jednotlivých testů. Pokud je\ uveden pouze parametr `all`, generátor vygeneruje všechny sady testů. Skript `run.py` slouží ke\ spouštění testů. Akceptuje jeden až\ několik parametrů při spuštění, což jsou názvy sad testů, které bude postupně spouštět a\ ukládat do\ složek časy běhů.

Demonstrační program přijímá při spuštění několik parametrů. Většina parametrů se\ dělí na\ ty, které využívá serverová část, a\ na\ ty, které využívá klientská část programu. Jediným společným parametrem je:

`-p` *`<port-number>`*

:   \ \
    Nastaví číslo portu, na\ kterém se\ bude komunikovat. Pokud se\ neuvede, použije se\ hodnota *41813*.

# Serverové přepínače

`d`, `daemon`

:   \ \
    Spustí program v\ režimu serveru. Pokud není uveden, program se\ spouští v režimu klienta.

`-l` *`<log-file>`*

:   \ \
    Nastaví zapisování událostí do logů. Pokud není uvedený, logování neprobíhá.

`--no-detach`

:   \ \
    Spustí server, ale bez démonizace.

# Klientská část

`-h` *`<host-file>`*

:   \ \
    Soubor se\ seznamem strojů, které mají být zapojeny do\ výpočtu. Na\ každém řádku souboru se\ musí vyskytovat právě jedna adresa stroje v\ síti. Povinný parametr.

## Příkazy

Program akceptuje příkaz na\ kterékoliv pozici v\ přepínačích. Platí ten, který je\ uvedený jako poslední.

`s`, `start`

:   \ \
    Příkaz, kterým klient spustí serverové instance sama sebe na\ požadovaných vzdálených strojích. K\ přístupu využívá *ssh* přístup a\ funguje pouze na\ OS GNU/Linux.

`t`, `status`

:   \ \
    Příkaz, pomocí něhož klient zjistí, v\ jakém stavu se\ nechází který stroj uvedený v\ seznamu strojů.

`q`, `shutdown`

:   \ \
    Klient zašle všem strojům ze\ seznamu příkaz `shutdown`.

`k`, `forceShutdown`

:   \ \
    Klient zašle všem strojům ze\ seznamu příkaz `ForceShutdown`.

`rs`, `restart`

:   \ \
    Ekvivalent volání s\ příkazem `k` a\ `s`.

`r`, `reset`

:   \ \
    Klient zašle všem strojům ze\ seznamu příkaz `forceReset`.

`c`,`client`

:   \ \
    Program se\ má chovat jako klient a\ začít výpočet. Není třeba uvádět, pokud nen9 uveden jiný příkaz.

## Výpočty

`table`

:   \ \
    Ladící výpis -- tabulka otevřených socketů mezi procesy

`load`

:   \ \
    Spustí test škálování ve\ variantě *Shared*.

`ping`

:   \ \
    Spustí test latence ve\ variantě *Shared*.

`shared`

:   \ \
    Explicitně specifikuje variantu *Shared*.

`dedicated`

:   \ \
    Explicitně specifikuje variantu *Dedicated*.

`long`

:   \ \
    Nastavuje délku zprávy na\ *1*\ KB.

## Přepínače

`-n` *`<N>`*

:   \ \
    Nastavuje počet vláken testovacího výpočtu. Ovlivňuje také počet otevřených datových spojení mezi procesy.

`-w` *`<N>`*

:   \ \
    Nastavuje množství práce.

`-s` *`<N>`*

:   \ \
    U\ testu latencí nastavuje způsob rozhodování o\ příjemci dotazu. U\ testu škálování nastavuje zpoždění při generování stavu.

