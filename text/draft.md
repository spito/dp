---
header-includes:
    - \usepackage{text/custom}
---

# Úvod

XXX

# Uvedení do problematiky

## Modelchecking

## Paralelizmus

Pojem paralelizmus lze přeložit jako souběžnost. V\ informatice tento pojem znamená, že\ nějaká aplikace provádí (zdánlivě) současně dvě nebo více nezávislých výpočtů. Paralelizmus můžeme dále rozdělit na\ dva druhy -- paralelizmus ve\ sdílené paměti a\ v\ distribuované paměti. V\ případě paralelizmu ve\ sdílené paměti hovoříme o\ vlákně jako o\ základním funkčním prvku. U\ paralelizmu v\ distribuované paměti je\ základním prvkem proces.

Ačkoliv téměř každý člověk, který se\ pohybuje v\ oblasti informatiky, intuitivně tuší, co\ znamenají pojmy proces a\ vlákno, uvedu zde popis, který dle mého názoru je\ dostačující.

> Proces je\ instance počítačového programu, který je\ právě vykonáván. Proces si\ žádá od\ operačního systému zdroje jako například paměť. Proces nemá přímou možnost komunikovat s\ dalšími procesy a\ pro komunikaci je\ třeba využít služeb operačního systému.

> Vlákno nejmenší sekvence příkazů, které mohou být nezávisle spravovány plánovačem úloh operačního systému. Vlákno je\ vždy součástí procesu a\ jako takové nevlastní žádné zdroje; ty\ náleží procesu. Protože spolu vlákna sdílí paměť, komunikace mezi nimi probíhá bez přímé interakce s\ operačním systémem za\ použití synchronizačních primitiv dostupných na\ dané architektuře.

Důvod, proč není dostupná formální definice procesu, je\ ten, že\ pojem proces uvedli v\ 60.\ letech návrháři systému Multics[[X]](http://www.cim.mcgill.ca/~franco/OpSys-304-427/lecture-notes/node4.html) jako něco víc obecného než úkol, to\ v\ kontextu více programové jednotky. Význam slova proces tak byl určen spíše implementací systému než zavedením formálního popisu, u\ čehož v\ současné době, kdy existuje několik operačních systémů s\ různou filozofií, zůstalo [[X]](https://msdn.microsoft.com/en-us/library/windows/desktop/ms684841%28v=vs.85%29.aspx)[[X]](http://www.linfo.org/process.html). Obdobná situace je\ v\ případě definice pojmu vlákno, třebaže lze dohledat uvedení pojmu [[X]](https://en.wikipedia.org/wiki/Thread_%28computing%29#cite_note-1). Nicméně zde platí stejně jako u\ procesu, že\ přesný popis, co\ vlákno je, se\ liší na základě operačního systému, případně běhovém prostředí[^run-environment]

[^run-environment]: Běhové prostředí je\ sada knihoven a\ nástrojů, které bývají součástí vyšších programovacích jazyků jako třeba Java nebo C#. Běhové prostředí může definovat jinou sémantiku především pro vlákna, které může být rozdílné od\ sémantiky, kterou popisuje operační systém.

V\ případě paralelizmu ve\ sdílené paměti programátor očekává, že\ po\ startu programu má k\ dispozici jedno hlavní vlákno, ze\ kterého následně v\ případě potřeby pouští další vlákna, na\ která většinou na\ konci programu hlavní vlákno zase počká. V\ průběhu výpočtu používají vlákna synchronizační primitiva pro vzájemnou komunikaci. Navíc platí, že\ programátor má volnou ruku v\ tom, zda spustí jednu a\ tu\ samou funkcí vícekrát ve\ více vláknech nebo spustí v\ některých vláknech jiné funkce.

V\ případě distribuovaného výpočtu je\ situace zcela opačná. Jeden program je\ s\ pomocí služeb operačního systému nebo nějaké knihovny spuštěn na\ různých výpočetních strojích, přičemž počet strojů -- a\ tedy počet procesů -- se\ během algoritmu zpravidla nemění. V\ průběhu výpočtu spolu mohou procesy komunikovat pomocí rozhraní, které jim poskytují podpůrné nástroje. Většinou je\ komunikace vedena formou zasílání zpráv. Je\ možné, že\ operační systém nebo knihovna umožňuje mezi procesy sdílet i\ části paměti, z\ důvodů výrazného zpomalení a\ problematické synchronizace nebývá její použití časté.

Z\ předchozích odstavců vyplývá, že\ přístup obou druhů paralelizmu je\ odlišný. Nejedná se\ tedy o\ konkurující, ale naopak doplňující nástroje, které je\ možné využít při návrhu aplikace. Liší se i\ nástroje, pomocí kterými se\ oba druhy paralelizmu řídí. Pro paralelizmus ve\ sdílené paměti se\ používají nástroje jako [POSIX vlákna](http://pubs.opengroup.org/onlinepubs/9699919799/functions/V2_chap02.html#tag_15_09) nebo [OpenMP](http://openmp.org). Paralelizmus v\ distribuované paměti zase využívají knihovny jako [MPI](http://www.mpi-forum.org) nebo [PVM](http://www.csm.ornl.gov/pvm/). Není proto nezvyklé, že\ jsou v\ rámci jedné aplikace použity oba druhy paralelizace.

## DIVINE

## Cíl práce

# Komunikační vrstva

Program DIVINE dokáže zpracovávat stavový prostor s\ použitím obou druhů paralelizace, z\ nichž je\ v\ aktuální verzi programu (DIVINE 3.x) upřednostňován režim paralelizace pouze ve\ sdílené paměti. Některé důvody jako například možnost komprese stavového prostoru -- a\ tudíž efektivnější využívání paměti -- nebo rovnoměrnější rozvržení pracovní zátěže jednotlivých vláken -- což vede k\ rychlejšímu prohledávání stavového prostoru -- jsou popsány v\ [Vláďova bakalářka] a\ v\ [moje bakalářka].

Druhý režim paralelizace je\ hybridní a\ zahrnuje oba dva druhy paralizace. Tento režim pochází ze\ starší verze programu (DIVINE 2.x) a\ oproti původní verzi nebyl nikterak vylepšován (až\ na\ malé optimalizační změny). Hybridní paralelizmus je\ realizován tak, že\ každý stav ze\ zpracovávaného stavového prostoru je\ staticky přiřazen některému vláknu na\ některé samostatné výpočetní jednotce pomocí hašování [odkaz na hash]. Jako komunikační vrstva je\ použit [standard MPI](http://www.mpi-forum.org/), konkrétně implementace [Open MPI](https://www.open-mpi.org).

Hlavní nevýhodou původní implementace hybridního paralelizmu bylo statické rozdělení stavů nejen mezi jednotlivé výpočtní stroje ale\ i\ mezi jednotlivá vlákna. Toto rozdělení má\ kromě nevýhody v\ potenciálně nerovnoměrném rozložení práce mezi jednotlivá vlákna i\ nevýhodu v\ nemožnosti použít aktuální implementaci komprese paměti.

Již\ v\ průběhu vytváření režimu paralelizace ve\ sdílené paměti bylo zřejmé, že\ by\ bylo možné upravit stávající hybridní režim tak, aby\ v\ rámci jednotlivých výpočetních jednotek byl\ použit režim paralelizace ve\ sdílené paměti, kdežto pro\ rozdělení práce mezi výpočetní jednotky by\ nadále používalo statické rozdělování stavů na\ základě haše. Tento režim, pracovně nazvaný dvouvrstvá architektura, ovšem z\ důvodu upřednostnění jiných úkolů nebyl nikdy realizována.

V současné době se\ pracuje na\ nové verzi programu DIVINE, přičemž součástí změn je\ i\ úprava modelu paralelního zpracování stavového prostoru a\ zavedení jednotného režimu paralelizace pomocí dvouvrstvé architektury. Z\ tohoto důvodu bylo zvažováno, jestli by\ jiná komunikační vrstva nebyla jednodušší na\ použití a\ jestli by\ nebyla efektivnější při\ práci s\ pamětí. Další věc, kterou jsem zvažoval, byla co\ nejmenší závislost na\ externích knihovnách.


**Removed:**
> Před volbou vhodného komunikačního rozhraní bylo potřeba definovat, v\ jakém prostředí bude program DIVINE spouštěn, a\ tedy jaká jsou hlavní kritéria výběru. Očekáváme, že [`TBA`]

## MPI

*Message Passing Interface*, zkráceně [MPI](http://www.mpi-forum.org/), je\ standardizovaný systém pro distribuované výpočty, což zahrnuje pro zasílání zpráv mezi procesy a\ prvky kolektivní komunikace. Ačkoliv se\ většinou mluví o\ systému, MPI jako takové je\ standard[^mpi-standard] a\ pro použití je\ potřeba použít některou z\ implementací.

[^mpi-standard]: Ačkoliv má MPI standard již  několik verzí, stále nedošlo ke\ standardizačnímu řízení u\ některé ze\ známých standardizačních autorit, jako je například [ISO](http://www.iso.org/) nebo [IEEE](https://www.ieee.org/).

Samotný standard MPI se\ zmiňuje o\ rozhraní pouze pro jazyky\ C a\ Fortran. Ve\ 2. verzi standardu MPI byla přidána podpora pro jazyk C++, která byla hned ve verzi 3 odstraněna. Implementace standardu MPI tak musí nabízet rozhraní v\ jazycích\ C a\ Fortran. Z\ těch základních jmenujme alespoň [MPICH](http://www.mpich.org/)[^mpich], [Open MPI](https://www.open-mpi.org), či\ LAM/MPI, jehož vývoj byl zastaven ve prospěch Open MPI. Pro jiné programovací jazyky existují moduly^[dle názvosloví daného jazyka také knihovny, balíčky, ...], které používají rozhraní pro jazyk\ C z\ dostupné implementace standardu MPI. Jde mimo jiné o\ programovací jazyky jako Java [[X]](https://en.wikipedia.org/wiki/Message_Passing_Interface#cite_note-16)[[X]](https://en.wikipedia.org/wiki/Message_Passing_Interface#cite_note-17)[[X]](https://en.wikipedia.org/wiki/Message_Passing_Interface#cite_note-18), Python[[X]](http://sourceforge.net/projects/pympi/)[[X]](https://en.wikipedia.org/wiki/Message_Passing_Interface#cite_note-10)[[X]](https://en.wikipedia.org/wiki/Message_Passing_Interface#cite_note-12), jazyk R[[X]](https://en.wikipedia.org/wiki/Message_Passing_Interface#cite_note-21), nebo o\ knihovny dostupné pro framework .NET[[X]](https://en.wikipedia.org/wiki/Message_Passing_Interface#cite_note-22)[[X]](https://en.wikipedia.org/wiki/Message_Passing_Interface#cite_note-23).

[^mpich]: MPICH byl vůbec první implementací MPI standardu, konkrétně MPI-1.1.

MPI je\ navržen jako multiplatformní systém, což umožňuje z\ pohledu aplikace nezohledňovat detaily architektury, jako jsou například endianita[^endianity] nebo rozsahy čísel. Kvůli tomu například zavádí MPI vlastní datové typy. Z\ pohledu výpočetních strojů nabízí MPI možnost dodat vlastní implementaci -- tzv. poskytovatele -- pro specifický hardware a\ dosáhnout tím lepšího výkonu distribuovaného výpočtu. Na\ běžných strojích v\ lokální síti běžně MPI používá jako poskytovatele síťovou infrastrukturu, v\ případě, že je distribuovaný výpočet spuštěn v\ rámci jednoho stroje, pak může využít možností operačního systému a\ mít poskytovatele pro sdílenou paměť.

[^endianity]: Způsob ukládání čísel do paměti.

### Princip používání MPI

Jako knihovna pro podporu paralelizmu v\ distribuované paměti poskytuje MPI několik nástrojů. Jsou jimi knihovna a\ hlavičkové soubory, které exportují deklarace funkcí a\ definice struktur, vlastní překladač, který způsobí připojení knihoven MPI k\ programu, a\ speciální zaváděcí program, pomocí které lze distribuovaný výpočet spustit.

Jakýkoliv program, který má být spuštěn pomocí MPI jako distribuovaný výpočet,musí nejprve inicializovat MPI knihovnu voláním funkce [`MPI_Init`](https://www.open-mpi.org/doc/v1.8/man3/MPI_Init.3.php). Následně je\ vhodné zjistit, co\ je\ každý proces zač, k\ čemuž slouží funkce [`MPI_Comm_rank`](https://www.open-mpi.org/doc/v1.8/man3/MPI_Comm_rank.3.php) a\ [`MPI_Comm_size`](https://www.open-mpi.org/doc/v1.8/man3/MPI_Comm_size.3.php). První z\ nich vrací rank procesu a\ druhá počet procesů. Rank je\ číslo, které udává pořadí procesu, je\ číslované od\ $0$ až\ po\ $N - 1$, kdy $N$ je\ počet procesů. Dále pokračuje běh výpočtu, který se\ obvykle řídí zasíláním zpráv. Před ukončením musí každý proces zavolat funkci [`MPI_Finalize`](https://www.open-mpi.org/doc/v1.8/man3/MPI_Finalize.3.php), která korektně ukončí spojení s\ ostatními procesy.

Pro spuštění programu je\ nejprve potřeba přeložit zdrojový kód a\ připojit k\ výslednému programu právě knihovnu MPI. To\ je\ možné udělat buď přeložením zdrojového kódu pomocí `mpicc` nebo `mpic++`, překladače dodaného implementací MPI standardu, nebo svému překladači nastavit vhodné systémové cesty právě ke knihovně MPI, aby ji\ mohl připojit k\ výslednému programu^[Což je\ ostatně přesně to\ samé, co\ provede překladač dodaný implementací MPI.].

Následné spuštění distribuovaného algoritmu se\ provede spuštěním zaváděcí program `mpirun` nebo `mpiexec`^[Jsou totožné.]. V\ rámci spuštění je\ potřeba definovat některé parametry, z\ nichž nejpodstatnější je\ seznam strojů, na\ kterých mají běžet procesy. Další parametry pak bývají volitelné a\ je\ možné jimi nastavit mnoho vlastností běhu.

### Koncepty MPI

MPI poskytuje různorodou schopnosti a\ nástroje pro řízení a\ běh distribuovaného výpočtu. Uvádím zde čtyři základní koncepty MPI, které byly uvedeny již v první verzi standardu. Ve\ verzi 2 přibylo dalších několik konceptů jako například sdílení paměti mezi procesy, dynamické vytváření procesů nebo podpora paralelních vstupně-výstupních operací. Protože ale patří tyto koncepty mezi pokročilé a\ obtížnější, nebudu je\ zde uvádět.

#### Komunikátor

Komunikátor ustanovuje skupinu procesů. Po\ spuštění programu jsou všechny procesy součástí skupiny `MPI_COMM_WORLD`. V\ průběhu výpočtu mohou procesy zakládat a\ rušit další skupiny, což může vést k\ dynamickému vytváření topologie v\ komunikaci. Každý proces připojením ke\ komunikátoru dostane další rank, který určuje jeho pořadí v\ rámci skupiny.

#### Jeden na jednoho

Jde o\ důležitý mechanizmus, který umožňuje posílat zprávy od\ jednoho procesu ke\ druhému. Komunikace jeden na\ jednoho je\ vhodná zvláště v\ případech nahodilé komunikace, kdy nelze dopředu říct, který proces zašle data kterému, nebo v\ případě, že\ je\ uskupení procesů do\ rolí pána a\ otroka -- jeden proces řídí část výpočtu nebo celý výpočet a\ jeden nebo více procesů přijímá příkazy a\ provádí výpočet. Zasílané zprávy je\ možné třídit pomocí štítku zprávy -- příjemce může definovat, zda ho\ zajímají zprávy s\ nějakým konkrétním štítkem, nebo všechny zprávy.

MPI nabízí několik variant zasílání zpráv, které se\ liší především svým vztahem k\ paměti a\ k\ blokování výpočtu. Jsou dvě základní rozdělení. První dělí operace na\ blokující a\ neblokující, druhé je\ dělí na\ přímé (nebafrované) a\ na\ ty s\ vyrovnávací pamětí (bafrované).

[`MPI_Send`](https://www.open-mpi.org/doc/v1.8/man3/MPI_Send.3.php) -- blokující nebafrované

:   \ \
    Funkce odešle zprávu a\ blokuje až\ do\ okamžiku, když přijímající proces započal příjem zprávy a\ zpráva byla úspěšně odeslána. Po\ skončení funkce jsou odkazovaná plně k\ dispozici.

[`MPI_Bsend`](https://www.open-mpi.org/doc/v1.8/man3/MPI_Bsend.3.php) -- blokující bafrované

:   \ \
    Funkce nakopíruje zprávu do\ vyrovnávací paměti, zahájí přenos zprávy a\ skončí. Po\ dokončení přenosu je potřeba obsah vyrovnávací paměti zlikvidovat.

[`MPI_Isend`](https://www.open-mpi.org/doc/v1.8/man3/MPI_Isend.3.php) -- neblokující nebafrované

:   \ \
    Funkce zahájí přenos zprávy a\ skončí. Zprávu nesmí odesílatel modifikovat, dokud si\ neověřil, že\ bylo odeslání dokončeno. To\ je\ možné provést voláním funkcí [`MPI_Test`](https://www.open-mpi.org/doc/v1.8/man3/MPI_Test.3.php) nebo [`MPI_Wait`](https://www.open-mpi.org/doc/v1.8/man3/MPI_Wait.3.php).

[`MPI_Ibsend`](https://www.open-mpi.org/doc/v1.8/man3/MPI_Ibsend.3.php) -- neblokující bafrované

:   \ \
    Funkce zahájí kopírování zprávy do\ vyrovnávací paměti, zahájí přenos zprávy a\ skončí. Zprávu nesmí odesílatel modifikovat, dokud si\ neověřil, že\ bylo odeslání dokončeno. To\ je\ možné provést voláním funkcí [`MPI_Test`](https://www.open-mpi.org/doc/v1.8/man3/MPI_Test.3.php) nebo [`MPI_Wait`](https://www.open-mpi.org/doc/v1.8/man3/MPI_Wait.3.php).

Pro příjem zprávy slouží primárně dvě funkce, opět dělené na\ blokující a\ neblokující.

[`MPI_Recv`](https://www.open-mpi.org/doc/v1.8/man3/MPI_Recv.3.php) -- blokující

:   \ \
    Funkce čeká na\ zprávu, dokud není zpráva doručena. Příchozí zpráva musí mít správného odesílatele a\ správnou hodnotu štítku, aby byla přijata, ale je možné ignorovat jak rank odesílatele, tak hodnotu štítku zprávy. Je\ třeba dopředu nastavit dostatečně velkou paměť pro příjem zprávy.

[`MPI_Irecv`](https://www.open-mpi.org/doc/v1.8/man3/MPI_IRecv.3.php) -- neblokující

:   \ \
    Funkce zahájí příjem zprávy, předá zpátky kontrolní strukturu a\ skončí. Příchozí zpráva musí mít správného odesílatele a\ správnou hodnotu štítku, aby byla přijata, ale je možné ignorovat jak rank odesílatele, tak hodnotu štítku zprávy. Příjem zprávy je\ potřeba ověřit voláním [`MPI_Test`](https://www.open-mpi.org/doc/v1.8/man3/MPI_Test.3.php) nebo [`MPI_Wait`](https://www.open-mpi.org/doc/v1.8/man3/MPI_Wait.3.php) s\ kontrolní strukturou jako parametrem. Je\ třeba dopředu nastavit dostatečně velkou paměť pro příjem zprávy. Pokud je\ iniciován příjem zprávy, je\ nutné tuto zprávu přijmout před ukončením výpočtu.

Pokud není dopředu známe, zda vůbec nějaká zpráva dojde, případně není známá její velikost, nabízí se\ použít dvou funkcí na\ zjištění příchozí zprávy. Funkce jsou opět dělené na\ blokující a\ neblokující.

[`MPI_Probe`](https://www.open-mpi.org/doc/v1.8/man3/MPI_Probe.3.php) -- blokující

:   \ \
    Funkce čeká, dokud nepřijde nějaká zpráva. Příchozí zpráva musí mít správného odesílatele a\ správnou hodnotu štítku, aby byla přijata, ale je možné ignorovat jak rank odesílatele, tak hodnotu štítku zprávy. Poté vrátí vlastnosti zprávy -- rank odesílajícího procesu, štítek zprávy a\ velikost zprávy v\ bytech.

[`MPI_Iprobe`](https://www.open-mpi.org/doc/v1.8/man3/MPI_Iprobe.3.php) -- neblokující

:   \ \
    Funkce zjistí, zda na\ příjem nečeká nějaká zpráva od\ správného odesílatele a\ se\ správnou hodnotu štítku. Je možné ignorovat jak rank odesílatele, tak hodnotu štítku zprávy. Pokud funkce zjistí příchozí zprávu, je\ možné zjistit její vlastnosti -- rank odesílajícího procesu, štítek zprávy a\ velikost zprávy v\ bytech.

#### Kolektivní komunikace

Kolektivní komunikace znamená, že\ všechny procesy začnou společně provádět nějakou operaci nad daty. Počet procesů je\ možné omezit vytvořením nového komunikátoru a\ provedením operace v\ něm. Ačkoliv je\ možné všechny kolektivní operace simulovat pomocí operací jeden na\ jednoho, je\ výhodnější použít přímo funkce pro kolektivní komunikaci, protože jejich použitím může MPI knihovna využít znalosti o\ topologii procesů a\ realizovat přenos dat efektivněji.

Mezi základní kolektivní operace patří jeden-všem, kdy jeden proces prošle stejná data všem ostatním procesům, všichni-jednomu, kdy všechny procesy až\ na\ jeden zašlou data jednomu procesu, přičemž data jsou před přijetím podrobeny redukční operaci, a\ bariéra, což je\ synchronizační primitivum, kterého musí všechny procesy ve\ skupině dosáhnout, než jim všem bude umožněno pokračovat dále ve\ výpočtu. Obdobně jako u\ komunikace jeden na\ jednoho i\ operace kolektivní komunikace mají varianty v\ podobě neblokujících volání; ověření dokončení operací se\ řeší také stejně.

[`MPI_Bcast`](https://www.open-mpi.org/doc/v1.8/man3/MPI_Bcast.3.php)

:   \ \
    Funkce realizující broadcast -- rozeslání balíku dat všem ostatním procesům ve\ skupině.

[`MPI_Reduce`](https://www.open-mpi.org/doc/v1.8/man3/MPI_Reduce.3.php)

:   \ \
    Funkce realizující redukci dat -- pošle data jednomu procesu od\ všech ostatních ze\ skupiny. Pro redukční operaci je\ možné využít buď jednu z\ předdefinovaných operací jako je\ součet, maximum, či\ logický součet, nebo si\ může uživatel definovat pomoc funkce [`MPI_Op_create`](https://www.open-mpi.org/doc/v1.8/man3/MPI_Op_create.3.php) vlastní operaci.

[`MPI_Barrier`](https://www.open-mpi.org/doc/v1.8/man3/MPI_Barrier.3.php)

:   \ \
    Funkce realizující bariéru.

Pokročilější kolektivní operace zahrnují rozesílání a\ sbírání různých dat od\ procesů ve\ skupině. Další operace jako [`MPI_Allgather`](https://www.open-mpi.org/doc/v1.8/man3/MPI_Allgather.3.php) nebo [`MPI_Allreduce`](https://www.open-mpi.org/doc/v1.8/man3/MPI_Allreduce.3.php) kombinují více kolektivních operací do\ jedné.

[`MPI_Scatter`](https://www.open-mpi.org/doc/v1.8/man3/MPI_Scatter.3.php)

:   \ \
    Funkce rozešle balíky dat všem procesům ve\ skupině. Rozdíl oproti funkci [`MPI_Bcast`](https://www.open-mpi.org/doc/v1.8/man3/MPI_Bcast.3.php) je\ v\ tom, že\ odesílající proces sestaví různá data pro každý přijímající proces.

[`MPI_Gather`](https://www.open-mpi.org/doc/v1.8/man3/MPI_Gather.3.php)

:   \ \
    Funkce přijme data od všech ostatních procesů ve\ skupině. Je\ podobná funkci [`MPI_Reduce`](https://www.open-mpi.org/doc/v1.8/man3/MPI_Reduce.3.php) s\ rozdílem, že\ není definována redukční operace, ale přijímající proces má k\ dispozici všechna data.

#### Vlastní datové typy

Standard MPI zavádí vlastní datové typy, které jsou schopné reprezentovat většinu základních datových typů v\ jazycích C, C++ a\ Fortran. Naopak některé MPI datové typy nemají reprezentaci ve\ jmenovaných programovacích jazycích. Jedním z\ důvodů zavedení vlastních datových typů je\ nezávislost na\ architektuře -- především na\ endianitě. Dalším důvodem je\ možnost používat předdefinované operace pro kolektivní komunikaci.

Kromě vlastních datových typů umožňuje MPI definovat i\ vlastní datové typy, což zahrnuje vytváření jak homogenních (pole), tak i\ heterogenních (struktury) datových typů. Spolu s\ uživatelsky definovanými operacemi je možné využít vlastní datové typy při redukcích během kolektivní komunikace.

## BSD sockety

Další možností je vlastní implementace komunikačního rozhraní pro DIVINE, které by\ používalo BSD sockety, které jsou v zahrnuty v [POSIX](http://pubs.opengroup.org/onlinepubs/9699919799/functions/contents.html) standardu. Vlastní implementace nepřidává žádnou závislost na externí knihovně a protože jsou BSD sockety v podstatě standardem pro síťovou komunikaci[dodat odkazy na pojednávající články], lze předpokládat, že výsledný kód bude možné bez větších změn použít i na operačních systémech, které nevycházejí z filozofie systému UNIX.

Popis BSD socketů je abstrakce nad různými druhy spojení. V\ části POSIX standardu o [socketech](http://pubs.opengroup.org/onlinepubs/9699919799/functions/V2_chap02.html#tag_15_10_06) můžeme nalézt poměrně nemálo věcí, které jsou specifikovány. Stěžejní z\ pohledu nástroje DIVINE a\ výběru vhodného komunikačního rozhraní jsou především dvě pasáže, a\ to\ `Address Famillies` a\ `Socket Types`. První definuje, skrz které médium se\ budou sockety používat, zatímco druhá podstatná pasáž je\ o\ tom, jaké vlastnosti bude mít samotný přenos dat skrz sockety.

Ačkoliv se\ v\ odkazované části POSIX standardu hovoří o\ ``Address Families'', dále ve\ standardu v\ části popisující funkce a\ jejich parametry se\ již hovoří o\ komunikační doméně. Budu tento termín nadále používat, neboť dle mého soudu lépe popisuje danou skutečnost.

### Komunikační doména

POSIX standard popisuje konkrétně tři možné komunikační domény -- lokální unixové sockety, sockety určené pro\ síťový protokol IPv4 [[RFC791]](https://tools.ietf.org/html/rfc791) a\ sockety pro síťový protokol IPv6 [[RFC2460]](https://tools.ietf.org/html/rfc2460). Standard se\ pak dále zabývá technickými detaily, jako které číselné konstanty jsou určeny pro kterou rodinu adres, či\ jak přesně jsou zápisy adres reprezentovány v\ paměti.

Pro\ adresaci unixových socketů se\ používá cesta adresářovým stromem,[^fs-tree] s\ tím, že\ standard nedefinuje maximální délku cesty. Žádné další aspekty adresování nejsou při práci unixovými sockety zohledňovány.

[^fs-tree]: Zde je nejspíše nutné malou poznámkou zmínit, že\ v\ souborových systémech unixového typu není adresářový strom stromem z\ pohledu teoretické informatiky, neboť umožňuje vytvářet cykly.

V\ případě socketů pro\ síťový protokol je\ třeba rozlišovat mezi protokolem IPv4 a\ IPv6. První zmiňovaný používá k\ adresaci čtyři čísla v\ rozsahu 0 -- 255. Při textové reprezentaci se\ ony\ čtyři čísla zapíšou v\ desítkovém základu a\ oddělí se\ tečkami. Ve\ strojovém zápisu se\ pak jedná o\ 32bitové číslo. Protokol IPv6 [[RFC4291]](https://tools.ietf.org/html/rfc4291) reflektuje primárně nedostatek adres IPv4 [[ICANN zpráva]](https://www.icann.org/en/system/files/press-materials/release-03feb11-en.pdf), což bylo při návrhu zohledněno a\ pro adresaci se\ používá osm čísel v rozsahu 0 -- 65 535. Preferovaná textová reprezentace je\ zapsat oněch osm čísel v\ šestnáctkovém základu a\ oddělit je\ dvojtečkami. Ve\ strojovém zápisu se\ pak jedná o 128bitové číslo.

Vzhledem k\ tomu, že\ v\ současné době oba protokoly koexistují vedle sebe a\ nelze bez podrobnějšího zkoumání rozhodnout, zda nějaká síť používá IPv4, či\ IPv6, je\ nutné v\ POSIXovém rozhraní nabídnout takovou funkcionalitu, která bude (pro běžné používáním) nezávislá na\ verzi protokolu IP. Touto funkcí je\ [`getaddrinfo`](http://pubs.opengroup.org/onlinepubs/9699919799/functions/getaddrinfo.html), která přeloží jméno cílového stroje a/nebo jméno služby a\ vrací seznam adres, lhostejno zda IPv4, či\ IPv6, které lze dále použít ve\ funkcích řešících otevření spojení, zaslání dat a\ jiných.

Jméno cílového stroje obyčejně bývá název stroje v\ síti, což může také zahrnout i\ nadřazené domény. Více informací lze hledat v\ [[RFC1034]](https://tools.ietf.org/html/rfc1034), [[RFC1035]](https://tools.ietf.org/html/rfc1035) (definují koncept doménových jmen a popisují jejich implementaci) a\ [[RFC1886]](https://tools.ietf.org/html/rfc1886) (rozšíření doménových jmen pro IPv6). Co je\ jméno služby bude probráno více u\ typů socketů.

### Typy socketů

[[POSIX standard]](http://pubs.opengroup.org/onlinepubs/9699919799/functions/V2_chap02.html#tag_15_10_06) definuje čtyři různé typy: `SOCK_DGRAM`, `SOCK_RAW`, `SOCK_SEQPACKET` a\ `SOCK_STREAM` s\ tím, že\ implementace může definovat další typy. Ne\ všechny typy socketů jsou k\ dispozici ve všech komunikačních doménách, navíc je\ nutné mít na paměti, že\ výsledné chování každého typu může navíc záležet na\ komuniační doméně. Pro nástroj DIVINE je\ navíc klíčové použití síťové komunikační domény, takže pozornost bude věnována primárně na\ chvání různých typů socketů v\ této doméně.

Stručné vysvětlení, co jednotlivé typy socketů znamenají:

*   `SOCK_STREAM` označuje spojité sockety.
*   `SOCK_SEQPACKET` označuje sekvenční sockety.
*   `SOCK_DGRAM` označuje nespojité sockety.
*   `SOCK_RAW` označuje socket, který je\ použit pro implementaci výše uvedených typů socketů. Nazvěme je\ jako hrubé sockety.

#### Spojité sockety

Jak vyplývá z\ názvu, spojité sockety vytváří spojení mezi dvěma komunikujícími stranami, přičemž spojení musí být ustanoveno před započetím výměny dat. POSIX standard dále vyžaduje, aby\ v\ případě zaslání dat bylo garantováno doručení nepoškozených dat ve\ správném pořadí. Další důležitá vlastnost je, že\ nelze klást omezení na velikost poslaných dat. Je\ tedy možné si\ spojité sockety představovat jako dvojtou linku, pomocí které lze\ odesílat libovolná data a\ ze\ které lze zároveň libovolná data přijímat.

##### Síťová doména

V\ síťové doméně je\ zbytečné rozlišovat mezi IPv4 a\ IPv6. Novější IPv6 sice přidává některé zajímavé vlastnosti jako například možnost šifrování, ale\ obecně lze\ z\ pohledu spojitých socketů brát IPv4 a\ IPv6 za\ totožné. Spojité sockety jsou v\ síťové komunikační doméně implementovány pomocí TCP [[RFC793]](https://tools.ietf.org/html/rfc793).

TCP v\ sobě implementuje všechny požadavky na\ spojité sockety, které klade POSIX standard. Před použitím je\ třeba navázat spojení, které operační systém provede ve\ třech krocích. V\ TCP je\ proud dat simulován tak, že\ jednotlivé bloky dat (dále jako záznamy) jsou rozděleny do paketů, v\ případě, že\ je\ záznam příliš velký na paket, je\ záznam rozdělen mezi více paketů.

Garance neporušení dat je\ realizována kontrolním součtem, který je\ předáván společně s\ daty v hlavičce paketu. Garance doručení a\ doručení ve\ správném pořadí je\ v\ TCP prováděna tak, že\ každý odeslaný paket má\ své unikátní číslo, případně pořadové číslo (v\ případě rozdělení záznamu do\ více paketů), a\ odesílající strana vyžaduje potvrzení přijetí od\ přijímající strany do\ určitého času [[RFC6298]](https://tools.ietf.org/html/rfc6298). Pokud by\ k\ potvrzení nedošlo, je\ paket odeslán znovu. Na\ straně příjemce je\ v\ případě rozdělení záznamu do\ více paketů záznam zrekonstruován.

Zde je\ nutné zmínit se\ o\ portech. Pro úspěšné navázání komunikace je\ zapotřebí dvou různých entit -- serveru (stroje, který očekává příchozí spojení) a\ klienta (stroje, který iniciuje komunikaci). V\ rámci serveru je\ obyčejně více aplikací, které čekají na\ příchozí spojení. Každé takové čekání musí být jednoznačně identifikováno a\ k\ tomu slouží port -- číslo, které popisuje jeden konec (ještě nenavázaného) spojení.

Klient musí dopředu vědět, na\ kterém portu chce na\ serveru navázat spojení, proto byl v\ počátcích internetu zveřejněn dokument [[RFC1700]](https://tools.ietf.org/html/rfc1700), který byl postupně aktualizován[^iana] a\ obsahoval původně seznam 256, posléze až\ 1024, obsazených portů pro\ specifikované účely, například porty 20 a\ 21 pro FTP [[RFC959]](https://tools.ietf.org/html/rfc959) nebo port 80 pro\ web a\ HTTP [[RFC7230]](https://tools.ietf.org/html/rfc7230). Číslo portu je\ v\ síťové doméně jméno služby a\ je\ předáváno jako parametr do\ funkce `getaddrinfo`.

[^iana]: V roce 2002 bylo původní [[RFC1700]](https://tools.ietf.org/html/rfc1700) nahrazeno za [[RFC3232]](https://tools.ietf.org/html/rfc3232), které odkazuje na online databázi na adrese <http://www.iana.org/assignments/protocol-numbers/>.

**[obrázek hlavičky paketu]**

##### Unixová doména

V\ unixové doméně lze\ spojité sockety chápat jako speciální soubory, které mohou být podobně jako soubory `FIFO`[^fifo] použity pro meziprocesovou komunikaci. Spojité sockety mají navíc od\ `FIFO` ty\ vlastnosti, že\ jsou obousměrné a\ že\ je\ možné je\ použít pro\ přenos zdrojů mezi procesy (například otevřené popisovače souborů).

[^fifo]: `FIFO`, neboli také `pipe` -- trubky, roury -- jsou speciální soubory, které se\ chovají jako jednosměrný proud dat. Jsou typicky na\ jednom konci otevřeny pro zápis a\ na\ druhém konci pro čtení.

Realizace garancí spojitých socketů je\ v\ případě unixové komunikační domény jednoduchá, za\ předpokladu korektní implementace a\ korektních prvků hardware, jako například pamětí, či\ pevných disků.

##### Ukázka použití

Z\ pohledu použití spojitých socketů v\ programu \v síťové komunikační doméně je\ potřeba odlišit kroky, které je\ třeba vykonat na straně serveru a\ na\ straně klienta pro\ navázání spojení.

**Server**

1.  Vytvoření socketu pomocí funkce [`socket`](http://pubs.opengroup.org/onlinepubs/9699919799/functions/socket.html).
2.  Zamluvení si\ portu pomocí funkce [`bind`](http://pubs.opengroup.org/onlinepubs/9699919799/functions/bind.html).
3.  Nastavení socketu jako připraveného a\ nastavení maximální fronty čekajících spojení funkcí [`listen`](http://pubs.opengroup.org/onlinepubs/9699919799/functions/listen.html).
4.  Čekání na\ příchozí spojení na\ připraveném socketu pomocí funkce [`accept`](http://pubs.opengroup.org/onlinepubs/9699919799/functions/accept.html).
5.  Po\ přijetí a\ zpracování příchozího spojení opakovat krok 4.

**Klient**

1.  Vytvoření socketu pomocí funkce [`socket`](http://pubs.opengroup.org/onlinepubs/9699919799/functions/socket.html).
2.  Požádání o spojení funkcí [`connect`](http://pubs.opengroup.org/onlinepubs/9699919799/functions/connect.html).

**[diagram]**

Po\ navázání spojení si\ jsou klient i\ server rovni a\ oba mohou používat funkce pro\ odesílání i\ přijímání dat. Spojité sockety je\ možné jak na\ jedné straně tak na\ druhé kdykoliv uzavřít, či\ přivřít. Zavřít socket je\ možné voláním funkce [`shutdown`](http://pubs.opengroup.org/onlinepubs/9699919799/functions/shutdown.html), která v\ případě TCP provede čtyřkrokové rozvázání spojení. Standardním voláním funkce [`close`](http://pubs.opengroup.org/onlinepubs/9699919799/functions/close.html) dojde k\ uvolnění popisovače souboru, ale\ až\ poslední volání nad\ daným socketem funkce `close` zavolá funkci `shutdown`.

#### Sekvenční sockety

Sekvenční sockety se\ od\ spojitých socketů mnoho neliší. Vytváří spojení mezi dvěma komunikujícími stranami a\ spojení musí být stejně tak\ ustanoveno před započetím výměny dat. Zůstávají stejné garance, jaké POSIX standard vyžaduje od\ spojitých socketů -- garance doručení a\ garance doručení nepoškozených dat.

V\ čem se\ ale liší, je\ sémantika přenosu dat. Spojité sockety vytvářely iluzi proudu dat, který hypoteticky nemusí skončit. Naproti tomu sekvenční sockety mají sémantiku jednotlivých zpráv. Každý blok dat, který je\ poslaný sekvenčním socketem, má nějakou omezenou velikost a\ příjemce na\ druhé straně si\ musí nejprve vyzvednout celý záznam, než může přijmout další zprávu. Funkce pro příjem zprávy tedy dokáží rozeznat hranice jednotlivých zpráv.

##### Síťová doména

POSIX standard nespecifikuje [[X]](http://pubs.opengroup.org/onlinepubs/9699919799/functions/V2_chap02.html#tag_15_10_18), jak má být tento typ socketu implementován v\ síťové komunikační doméně. Ačkoliv je\ možné předpokládat, že\ bude tento typ socketu dostupný v\ proprietárních sítích dostupný, v\ běžně používaných síťových prvcích není dostupný.

##### Unixová doména

Sekvenční sockety lze v unixové komunikační doméně taktéž chápat jako speciální soubory obdobně jako spojité sockety, přičemž z\ pohledu souborového systému jsou dokonce totožné. Implementaci sekvenčních socketů si\ není možné představovat jako obousměrné `FIFO` soubory, ale spíše jako frontu samostatných zpráv či\ balíčků dat. Obdobně jako u\ spojitých socketů je\ garance spolehlivosti závislá na\ korektní implementaci a\ bezchybnosti hardware.

Je\ vhodné na\ tomto místě podotknout, že\ ačkoliv jsou sockety implementovány pomocí souborů, nemusí to\ nutně znamenat, že\ klient i\ server musí běžet na\ stejném stroji. Souborový systém může být například sdílený v\ rámci vnitřní sítě.

##### Ukázka použití

Použití je vesměs totožné jako u\ spojitých socketů, ovšem s\ tím rozdílem, že\ namísto portu je\ potřeba uvést cestu v\ adresářovém stromě, čímž se\ ve\ specifikované složce vytvoří speciální soubor typu sekvenční socket. Aplikace, které chtějí znovupoužít ten samý socket, pak musí řešit problém kdy smazat soubor -- jestli před zamluvením socketu, nebo při skončení programu.

Vzhledem k\ nemožnosti použít sekvenční sockety v\ síťové komunikační doméně je\ ukázka platná pro unixovou komunikační doménu. Zde je\ uvedena varianta z\ pohledu serveru, kdy se případný soubor smaže při startu.

**Server**

1.  Vytvoření socketu pomocí funkce [`socket`](http://pubs.opengroup.org/onlinepubs/9699919799/functions/socket.html).
2.  Smazání případného souboru funkcí [`unlink`](http://pubs.opengroup.org/onlinepubs/9699919799/functions/unlink.html).
3.  Zamluvení si\ portu pomocí funkce [`bind`](http://pubs.opengroup.org/onlinepubs/9699919799/functions/bind.html).
4.  Nastavení socketu jako připraveného a\ nastavení maximální fronty čekajících spojení funkcí [`listen`](http://pubs.opengroup.org/onlinepubs/9699919799/functions/listen.html).
5.  Čekání na\ příchozí spojení na\ připraveném socketu pomocí funkce [`accept`](http://pubs.opengroup.org/onlinepubs/9699919799/functions/accept.html).
6.  Po\ přijetí a\ zpracování příchozího spojení opakovat krok 5.

**[diagram]**

#### Nespojité sockety

Dalším typem socketů jsou nespojité sockety. Na rozdíl od\ dříve popisovaných typů socketů neslouží k\ vytváření spojení a\ zároveň nedávají žádné garance na\ přenášená data. Protože POSIX standard vyžaduje, aby ke\ každé operaci zápisu do\ nespojitého socketu náležela právě jedna operace čtení ze\ socketu, může dojít dokonce k\ tomu, že\ ačkoliv jsou data úspěšně doručena, může příjemce chybně nastavenou operací čtení ze\ socketu způsobit zahození části dat.

Garance na\ nespojité sockety jsou dokonce tak slabé, že\ dle POSIX standardu je\ legitimní data vůbec neodeslat a\ zároveň nereportovat žádnou chybu -- například protože operační systém zapsal data do\ vyrovnávací paměti, vrátil řízení zpět programu a\ až\ poté se\ neúspěšně pokusil data odeslat. Toto chování by\ ovšem nemělo být časté.

Protože nespojité sockety nevytváří spojení, celý komunikační model je\ diametrálně odlišný od\ předchozích dvou typů socketů. Každý potenciálně komunikující program si\ proto musí vytvořit socket, na\ kterém bude poslouchat, a\ který se\ ze\ sémantického hlediska chová spíše jako poštovní schránka. Co\ se\ týká odesílatele, jedinou možností, jak zjistit odkud byla zpráva poslána, je\ uložit si\ adresu při přijetí zprávy. Tato možnost je\ sice možná také u\ spojitých a\ sekvenčních socketů, nicméně je\ poněkud zbytečné ji\ používat, neboť socket nemůže během své existence změnit ani jednu komunikující stranu.

##### Síťová doména

Podobně jako u\ spojitých socketů je\ v\ kontextu síťové domény zbytečné rozlišovat mezi IPv4 a\ IPv6. Jediný podstatnější rozdíl bude zmíněn později. Nespojité sockety jsou v\ síťové komunikační doméně implementovány pomocí UDP [[RFC768]](https://tools.ietf.org/html/rfc768).

UDP slouží k\ co\ nejjednoduššímu předávání dat od\ odesílatele k\ příjemci. Z\ tohoto důvodu v\ sobě implementuje minimum mechanismů, jako například garance doručení, neporušení integrity dat a\ jiné, které jsou implementované v TCP. Shodně s\ TCP je\ v\ případě UDP přenos realizován pomocí paketů.

Vzhledem k\ minumu protokolových mechanismů je\ hlavička UDP paketu velmi krátká a\ obsahuje pouhé čtyři údaje:

1.  Zdrojový port
2.  Cílový port
3.  Délka dat
4.  Kontrolní součet dat.

Zde je\ zajímavá položka kontrolního součtu dat. Dle RFC768 sice může hlavička paketu obsahovt kontrolní součet dat, ovšem nikde není specifikováno, co se\ má dít v\ okamžiku, kdy příjemce zjistí, že\ se\ kontrolní součet přijatých dat neshoduje se\ součtem uvedeným v\ hlavičce.

Další zajímavou položkou z\ hlavičky UDP paketu je\ délka dat. Při znalosti této informace by\ bylo možné připravit přesně velkou paměť, do\ které by\ se data přijala. Rozhraní, které popisuje POSIX standard[^posix-recv] ale\ takovou možnost nenabízí, resp. funkce vrací tuto hodnotu až\ poté, co došlo k\ přečtení zprávy. Je\ tedy nutné při navrhování protokolu, který bude používat UDP, zvážit nejvyšší možnou délku zprávy.

Vzhledem k\ tomu, že\ UDP nevytváří spojení, je\ nutné při každém zaslání zprávy specifikovat příjemce. Jedním z\ vedlejších důsledků této vlastnosti je, že\ je možné zprávu zaslat na\ speciální adresu, což způsobí, že\ se\ zpráva rozešle všem síťovým prvkům v\ lokální síti. V\ IPv4 se\ tento způsob doručování zpráv jmenuje *broadcast* a\ ona speciální adresa je\ `255.255.255.255`. V\ IPv6 se\ nehovoří o broadcastu, ale o\ *multicastu*[^multicast]. Multicast se\ od\ broadcastu liší tím, že\ zpráva nemusí být zaslána všem strojům v\ místní síti, ale\ pouze těm, kteří se\ nahlásí o\ její přijetí. Zpráva zaslaná pomocí multicastu navíc může být doručena i\ mimo lokální síť.

[^multicast]: I\ v\ IPv4 byl později [[RFC988]](https://tools.ietf.org/html/rfc988)[[RFC3376]](https://tools.ietf.org/html/rfc3376) také implementován multicast, ovšem koncovému uživateli Internetu nemusí být dostupný.

[^posix-recv]: Jedná se primárně o funkce [`recv`](http://pubs.opengroup.org/onlinepubs/9699919799/functions/recv.html), [`recvfrom`](http://pubs.opengroup.org/onlinepubs/9699919799/functions/recvfrom.html) a [`recvmsg`](http://pubs.opengroup.org/onlinepubs/9699919799/functions/recvmsg.html).

##### Unixová doména

Stejně jako předchozí typy socketů jsou\ i\ nespojité sockety reprezentovány pomocí souboru v\ souborovém systému. Ačkoliv i\ v\ unixové komunikační doméně platí stejně slabé garance, lze si\ například pohledem do\ zdrojového kódu implementace GNU/Linuxu[^linux-source-sockets] verze 4.3 všimnout, že\ například zápis do\ sekvenčního socketu je implementován pomocí stejné funkce jako zápis do\ nespojitého socketu. Z\ toho vyplývá, že\ nespojité sockety někdy mohou mít stejné garance jako sekvenční sockety.

[^linux-source-sockets]: Funkce `unix_dgram_sendmsg` a `unix_seqpacket_sendmsg` na <http://lxr.free-electrons.com/source/net/unix/af_unix.c>.

##### Ukázka použití

Oproti předchozím dvěma ukázkám použití je\ tento typ socketů diametrálně odlišný. Vzhledem k\ faktu, že\ nedochází k\ ustanovení spojení, zde není rozdělení na\ server a\ na\ klienta, ale pouze na\ komunikující subjekty.

Ukázka použití v unixové komunikační doméně[^note-site]:

1.  Vytvoření socketu pomocí funkce [`socket`](http://pubs.opengroup.org/onlinepubs/9699919799/functions/socket.html).
2.  Smazání případného souboru funkcí [`unlink`](http://pubs.opengroup.org/onlinepubs/9699919799/functions/unlink.html).
3.  Zamluvení si\ portu pomocí funkce [`bind`](http://pubs.opengroup.org/onlinepubs/9699919799/functions/bind.html).
4.  Čekat na\ příchozí zprávu pomocí funkcí [`select`](http://pubs.opengroup.org/onlinepubs/9699919799/functions/select.html) či [`poll`](http://pubs.opengroup.org/onlinepubs/9699919799/functions/poll.html).
5.  Přečíst a zpracovat zprávu.

**[diagram]**

[^note-site]: Pro nasazení do\ síťové komunikační domény by\ postačilo vynechat krok 2 -- smazání případného souboru.

#### Hrubé sockety

Hrubé sockety[^raw-socket] jsou posledním typem socketů, které definuje POSIX standard. Sami o\ sobě nejsou samostatným typem (pokud by\ byly srovnávány s\ třemi předchozími), ale\ spíše je\ základem pro\ dříve jmenované. Žádostí o\ vytvoření hrubého socketu program žádá rozhraní operačního systému pro přímý přístup ke třetí vrstvě [ISO/OSI](http://www.ecma-international.org/activities/Communications/TG11/s020269e.pdf) modelu.

Touto cestou je\ možné implementovat libovolný vlastní protokol na\ čtvrté vrstvě ISO/OSI modelu. Moderní operační systémy z\ důvodu bezpečnosti nedovolují aplikacím s\ běžným oprávněním vytváření takovýchto typů socketů, proto se\ jimi nebudu v\ této práci dále zaobírat.

[^raw-socket]: V originálu *raw sockets*.

## Asio

[Asio](http://www.boost.org/doc/libs/1_60_0/doc/html/boost_asio.html) je\ knihovna z\ Boostu, která nabízí asynchronní vstupně-výstupní operace. Je\ také jednou z\ knihoven, které mohou být použity i\ [samostatně](http://think-async.com).

### Boost

[Boost](http://www.boost.org) je\ seskupení C++ knihoven, které pokrývají mnoho témat, od\ základních věcí jako časovače, přes statistické funkce, až\ po\ práci s\ obrázky či\ regulárními výrazy. Celkově tak Boost obsahuje několik desítek takových khihoven. Některé knihovny navíc mohou existovat ve\ dvou formách -- jednou jako součást Boostu, podruhé jako samostatná knihovna.

Mnohé knihovny z\ Boostu, obdobně jako nemalé části Standardní C++ knihovny[^STD] (dále jako STD), nabízejí sjednocené rozhraní k\ systémovému rozhraní, které se\ může lišit na\ základě operačního systému. Jako příklad co\ nejrozdílnějšího systémového rozhraní lze uvést operační systémy [Microsoft Windows](https://www.microsoft.com/cs-cz/windows) a\ UNIX-like[^unix-like] systémy. Knihovny z\ Boostu, na\ rozdíl od\ STD, mnohdy poskytují specifická rozšíření pro některé architektury a\ operační systémy.

[^STD]: Standardní C++ knihovna je\ kolekce tříd a\ funkcí, které jsou napsány převážně v\ jazyce C++ (některé části, například Knihovna jazyka C, jsou psány v\ jazyce C). Standardní C++ knihovna je\ součástí ISO C++ standardu, který tak definuje rozhraní a\ další vlastnosti knihovních tříd a\ funkcí. Více o\ standardní knihovně lze nalézt na <http://en.cppreference.com/w/cpp/links> a\ v\ odkazech uvedených na\ stránce.

[^unix-like]: Jedná se\ o\ operační systémy, které vycházejí z\ filozofie systému [UNIX](https://en.wikipedia.org/wiki/Unix#cite_note-Ritchie-3), poskytují obdobné rozhraní, ale neprošly procesem standardizace. Jako příklad lze uvést operační systémy jako BSD, či\ System\ V.

Vztah knihoven obsažených v\ Boostu a\ STD je\ v\ některých případech velmi těsný. Důvod je\ ten, že spousta tříd a\ funkcí, které jsou standardizovány a\ tedy jsou součástí STD, mají svůj původ v\ některé knihovně v\ Boostu [[X]](http://www.boost.org/doc/libs/?view=filtered_std-proposal)[[X]](http://www.boost.org/doc/libs/?view=filtered_std-tr1). V\ novějších verzích standardu jazyka C++ pak těchto knihoven přibývá.

### Knihovna Asio

 Hlavním přínosem knihovny Asio je její pojetí asynchronních volání. Asynchronní volání se\ v\ posledních několika letech stávají populární především možností použít ho v\ rozšířených programovacích jazycích, jako je\ například  [Java](https://docs.oracle.com/javase/6/docs/api/java/util/concurrent/FutureTask.html) nebo [C#](https://msdn.microsoft.com/en-us/library/hh873175%28v=vs.110%29.aspx). V\ poslední velké revizi jazyka C++ se\ asynchronní volání objevily také v\ podobě funkce `std::async`. Asynchronní volání ve\ zkratce znamená, že\ namísto běžného volání funkce poznačíme, že\ požadujeme provedené té které funkce, a\ až v\ místě, kde potřebujeme znát výsledek, si\ o\ něho požádáme. Je\ pak na\ možnostech jazyka a\ běhového prostředí, aby se\ postaralo o\ vyhodnocení asynchronně volané funkce. Možností je\ zde více, namátkou například spuštění asynchronní funkce v\ samostatném vlákně, nebo prolnutí funkcí během překladu do\ strojového kódu či\ mezikódu.

Další neméně důležitou součástí jsou vstupně-výstupní operace. STD poskytuje rozhraní pro práci se\ standardním vstupem a\ výstupem a\ také rozhraní pro práci se\ soubory. Co\ už\ ale STD knihovna nenabízí je\ práce se\ sítí, kterou naopak knihovna Asio poskytuje. Poskytnuté funkce a\ třídy jsou navíc psány v\ obdobných konvencích jako STD.

Rozhraní pro síťovou komunikaci implementuje knihovna Asio nad BSD sockety. Samozřejmě poskytuje podporu pro použití obou verzí IP, taktéž pro TCP i\ UDP. Navíc, protože je\ v\ současné době často nutnost použít zabezpečené spojení, umožňuje knihovna Asio použít SSL [[RFC6101]](https://tools.ietf.org/html/rfc6101), ke\ kterému je\ ale potřeba další knihovna -- [OpenSSL](https://www.openssl.org/).

### Princip používání Asio

Knihovna Asio je\ postavena na\ návrhovém vzoru Proaktor[D. Schmidt et al, Pattern Oriented Software Architecture, Volume 2. Wiley, 2000.]. Základem pro veškeré asynchronní operace je\ objekt třídy `asio::io_service`, která zároveň plní úlohu proaktora. Další třídy, které reprezentují například sockety, mají roli inicializátorů. Poslední rolí, která stojí za\ zmínku, je\ oznamovač, což je\ funkce, která je\ spuštěna proaktorem po\ dokončení asynronní operace.

Asynchronní režim se\ za\ podpory knihovny Asio používá poměrně jednoduše. Po\ vytvoření instance třídy `asio::io_service` program zaregistruje různé události, které ho zajímají. Může to\ být impulz z\ časovače, příchozí spojení nebo třeba dokončený zápis dat do\ souboru. Současně s\ registrací program upřesňuje, jaké funkce se\ mají zavolat v\ okamžiku, kdy událost nastane. Jediné, na\ co\ je\ třeba dávat pozor, je\ opětovná registrace na\ nastalou událost. Po\ zaregistrování všech požadovaných událostí stačí spustit metodu `run` na\ proaktoru.

Třídy z\ knihovny je\ možné použít i\ pro synchronní vstupně-výstupní operace, pokud budou jejich metody volat přímo a\ nikoliv skrze proaktora. Protože ale spousta tříd ke\ svému vytvoření vyžaduje objekt třídy `asio::io_service`, jejímu vytvoření se\ nevyhneme.

## Stávající komunikační rozhraní

XXX

# Návrh a implementace nového komunikačního rozhraní

Na\ základě vlastností jednotlivých dříve uvedených přístupů jsem se\ rozhodl, že\ implementuji novou komunikační vrstvu za\ použití BSD socketů. Pro samotnou implementaci pak bude potřeba ze\ dříve uvedených typů socketů vybrat ten nejvhodnější. Nová implementace dále vyžaduje vytvořit jednoduchý komunikační protokol pro ustanovení sítě strojů kooperujících na\ distribuovaném výpočtu. Při návrhu nové implementace se\ navíc nemusím držet architektury distribuované aplikace, jak ji\ popisuje MPI, která má dle mého názoru některé vady, pročež jsem se\ rozhodl, že vytvořím architekturu s\ jinými vlastnostmi.

Při zvažování, zda implementovat vlastní zjednodušenou nadstavbu nad BSD sockety, nebo zda použít knihovnu Asio, jsem zvolil první možnost. Jako důvod uvádím, že\ použití knihovny Asio by\ zavedlo do\ projektu další závislost -- buď ve\ formě správné verze knihovny Boost na\ straně uživatele, nebo ve\ formě nutnosti dodávat zdrojové soubory knihovny Asio spolu se\ zdrojovými soubory nástroje DIVINE, přičemž obojí přináší režii do\ správy projektu, který je\ limitován lidskými zdroji. Druhým důvodem pak může být, že\ z\ možností knihovny Asio by\ byla v\ nástroji DIVINE využito jen malá část.

Nová implementace bude nasazena na\ výpočetním stroji, jehož architektura je [x86-64](http://www.amd.com/Documents/x86-64_wp.pdf) a\ jehož operační systém používá rozhraní definované POSIX standardem -- převážně počítám s\ operačním systémem [GNU/Linux](http://www.gnu.org/gnu/linux-and-gnu.html.en). Dále předpokládám, že\ endianita všech strojů participující na\ výpočtu je\ stejná.

## Analýza vlastností typů socketů

Pro volbu správného typu socketů je\ třeba pečlivě zvážit vlastnosti jednotlivých typů, neboť následná implementace se\ může velmi lišit. Cílem je\ implementovat síťovou komunikační vrstvu do\ nástroje DIVINE, takže nebudu rozebírat vlastnosti socketů v\ unixové komunikační doméně.

### Spojité sockety

Spojité sockety mají několik vlastností, ať\ už\ dobrých, či\ horších, které je\ třeba vzít v\ potaz:

*   udržování spojení
*   možnost mít více spojení mezi výpočetními stroji
*   garance doručení nepoškozených dat
*   možnost poslat neomezeně dlouho zprávu
*   režie spojená s\ udržováním spojení a\ garancemi

#### Udržování spojení

Distribuované výpočty, které nástroj DIVINE provádí, vyžadují, aby\ žádný z\ participatů výpočtu nepřerušil kontakt s\ ostatními. To\ se\ může stát jednak chybou v\ síti -- rozpojením sítě --, jednak zastavením výpočtu na\ stroji, ke\ kterému může dojít například z\ důvodu chyby v\ programu. U\ spojitých socketů jsou při\ přerušení spojení notifikování oba\ participanti komunikace, tudíž může dojít k\ následnému korektnímu ukončení distribuovaného neukončeného výpočtu.

Nevýhodou je\ nutnost pravidelné výměny zprávy, která říká, že\ spojení je\ stále aktivní, mezi participanty. Tato výměna je\ ale nezbytná pouze v\ případě, že\ v\ mezičase nebyla mezi participanty komunikace zaslána žádná zpráva. Vzhledem k\ předpokladu, že\ mezi výpočetními stroji bude docházet k\ čilé výměně dat, nepokládám tuto nevýhodu za\ kritickou.

#### Více spojení

Mezi dvěma komunikujícími stroji je\ v\ případě spojitých socketů možné navázat více než jedno spojení v\ rámci jednoho obslužného portu na\ straně serveru. Tento aspekt je\ možné vypozorovat ze\ sekvence kroků, které vykonává server k\ přijímání a\ zpracování příchozách zpráv, které byly prezentovány u\ příkladu použití. Ustanovení více souběžných spojení může být\ potenciálně výhodné, neboť každé takové spojení můžeme chápat jako komunikační kanál a\ každý kanál použít pro\ jiný účel.

Lze tak například vytvořit jeden kanál pro\ posílání řídících zpráv, další kanály pak\ pro\ posílání dat. Výhodou navíc může být, že\ každé vlákno nástroje DIVINE bude obsluhovat svůj vlastní datový kanál bez nutnosti explicitního zamykání přístupu ke komunikačnímu zdroji.

Vzhledem k\ tomu, jak nástroj DIVINE pracuje v\ distribuovaném výpočtu s\ daty, které posílá na\ zpracování a\ které naopak přijímá,a\ vzhledem k\ očekávaným velikostem datových balíčků, je\ výhodné se\ vyhnout častému kopírování dat. Přístup více spojení přenechává operačnímu systému rozhodování o\ přidelěných vyrovnávacích pamětech, což\ se\ může projevit v\ efektivnější práci s\ pamětí, protože bude docházet k\ menšímu počtu nutného kopírování bloků dat.

#### Garance doručení nepoškozených dat

Nástroj DIVINE potřebuje ke\ své práci korektní data. V\ případě běhu ve\ sdílené paměti je\ jednoduché zaručit, že\ se\ při manipulaci s\ daty nestane, aby byly nějak poškozeny. V\ distribuovaném prostředí, kdy je\ pro přenos dat mezi výpočetními uzly třeba použít síťové rozhraní, nastupuje mnoho hardwarových prvků, z\ nichž každý může ovlivnit konzistenci přenášených dat.

Spojité sockety, konkrétně implementované pomocí TCP, tyto garance zajišťují pomocí kontrolních součtů dat při příjmu. Pokud je\ kontrolní součet dat odlišný od\ toho, který byl v\ hlavičce TCP paketu, dojde k\ opětovnému přenosu paketu. Použitím spojitých socketů tak odpadá potřeba implementovat vlastní mechanismy do\ nástroje DIVINE, které by\ zaručovaly nepoškozená data.

TCP sockety mají navíc mechanismus, který zaručuje (v\ případě, že\ to\ je\ možné) doručení dat. Znamená to\ tedy, že\ pokud do\ určitého časového intervalu nedorazí odesílateli potvrzení o\ doručení, pošle odesílatel zprávu znovu.

#### Možnost poslat neomezeně dlouhou zprávu

TCP vytváří abstrakci, že\ přenášená data jsou souvislý proud dat. Protože TCP pakety mají omezenou maximální velikost, dochází při poslání velkého bloku dat k\ rozdělení záznamu do více paketů, které jsou samostatně poslány sítí a\ na\ straně přijemce opět poskládány. Na\ toto se také vztahuje garance nepoškozených dat, která je implementována tak, že jsou pakety tvořící jeden blok dat sekvenčně očíslovány, takže případně prohození pořadí paketů zachytí příjemce a\ dle čísel poskládá správné pořadí.

Tato vlastnost TCP je\ z\ jedné strany výhodou, protože není třeba v\ samotné aplikaci řešit, zda se\ data vejdou do\ paketu, či\ zda je\ nutné je\ rozdělit a\ posléze spojit -- výhodou je, že\ není třeba implementovat vlastní mechanismus. Nevýhodou pak může být případně zdržení, neboť ztracený či\ poškozený paket zdržuje nejen celou zprávu, ale\ celý jeden směr komunikačního spojení, protože nemůže být přenesen jiný, bezproblémový, blok dat.

#### Režie spojená s garancemi

Předem zmíněné výhody vyvažuje na\ druhé straně režije spojená jak s\ udržováním spojení tak se\ zasíláním opravných paketů. Udržování spojení je\ realizováno periodicky se\ opakujícím posíláním krátkého TCP paketu mezi komunikujícími stranami. Tento vyměňování zpráv má nějakou nezanedbatelnou režii, ovšem posílání krátkých paketů je\ nutné pouze v\ případě, že\ ono spojení nebude řádně vytěžováno -- každý poslaný TCP paket je\ totiž známkou toho, že\ spojení je\ stále udržováno.

Co\ se\ týká garancí na\ doručení, vyvstávají zde dva potenciální problémy. První z\ nich je, že\ operační systém si\ musí držet v\ paměti již odeslaná data, a\ to\ z\ důvodu případného opakování přenosu. Může se\ tedy docházet ke\ zbytečnému využívání paměti. Druhým problémem je\ nutnost potvrzení doručení paketu. Toto je opět možné realizovat samostatným krátkým paketem v\ případě zřídkavého datového provozu v\ opačném směru, nebo přidáním příznaku doručení do\ hlavičky paketu jdoucím po\ stejné lince zpět.

Z\ krátké analýzy vyplývá, že\ v\ případě zřídkavého zasílání dat dochází k\ režii, která zatěžuje nejen oba komunikující subjekty, ale\ také veškerou síťovou infrastrukturu mezi nimi. U\ nástroje DIVINE se\ nicméně očekává, že\ datový provoz mezi všemi participanty výpočtu bude velmi vysoký, takže síť nebude zbytečně zaplavována krátkými pakety. Každopádně platí, že\ v\ porovnání s\ UDP paketem má TCP paket výrazně větší hlavičku a\ tedy množství dat přenesených po\ čas komunikace pomocí TCP mezi dvěma subjekty úměrně tomu vzrůstá oproti komunikaci přes UDP.

### Nespojité sockety

Nespojité sockety také nabízejí zajímavé vlastnosti, které je\ vhodné rozebrat:

*   žádné garance
*   jednoduchý protokol
*   omezení na velikost zprávy
*   neudržování spojení

Ačkoliv mají nespojité sockety pokud možno vyvinout co\ největší úsilí k\ tomu, aby zpráva došla jejímu příjemci, nejsou dané žádné garance o\ tom, jestli data dojdou v\ pořádku, či\ zdali vůbec dojdou. Toto s\ sebou nese dva podstatné důsledky.

Jednak to\ znamená, že\ se\ bude v\ síťové komunikaci posílat méně dat v\ hlavičkách, což znamená, že\ by\ za\ stejný časový interval bylo možné poslat více zpráv. To\ navíc souvisí s\ další vlastností nespojitých socketů, kterou je\ jednoduchý protokol.

Pak to\ také znamená, že\ by bylo potřeba implementovat vlastní protokol nad UDP pro ověřování korektnosti dat a\ pro ověřování jejich doručení. Předpokládá se, že\ nástroj DIVINE bude provozován v\ uzavřených sítích, takže je\ zde silný předpoklad na\ to, že\ síťových prvků mezi dvěma komunikujícími stroji bude malý počet -- typicky jeden síťový rozbočovač. Případně implementovaný protokol by\ tedy měl být šetrný k\ běžnému provozu a\ až\ v\ případě chyby při přenosu by\ mělo dojít k vyšší režii.

Další vlastností nespojitých socketů je\ maximální délka zprávy. Maximální délku UDP paketů POSIX standard [`link na konkrétní hodnotu`] se\ uvádí jako 64\ KB minus několik bytů na\ určené pro hlavičky IP paketů a\ UDP paketů. Ačkoliv nástroj DIVINE posílá zprávy o\ maximální velikosti několik málo desítek kilobytů, mohlo by\ se\ stát, že\ je potřeba poslat delší zprávu, u\ které by\ pak bylo potřeba zajistit, aby\ ji\ příjemce správně poskládal dohromady.

Poslední vlastností, kterou jsem výše uvedl, je\ neudržování spojení. Zde je\ namístě krátká polemika o\ tom, jak je\ síťová komunikace využívaná z\ pohledu nástroje DIVINE. Ačkoliv totiž od komunikační vrstvy DIVINE požaduje v\ podstatě pouze posílání zpráv, kde by\ se\ nespojité sockety hodily, potřebuje také udržovat znalost o\ tom, že\ jsou všechny stroje participující na\ distribuovaném výpočtu spojené. Danou funkcionalitu UDP nenabízí a\ bylo by\ tedy nutné ji\ taktéž implementovat.

#### Požadavky pro protokol

Z\ výše uvedených vlastností UDP a\ nespojitých socketů vyplývají některé vlastnosti, které by\ případně implementovaný protokol měl zajišťovat. Jsou jimi:

1.  garance doručení zpráv
2.  garance doručení nepoškozených dat
3.  rozdělení dlouhé právy a její složení při příjmu
4.  detekce, zda jsou stroje stále spojeny

Složení příliš dlouhých zpráv z\ více UDP paketů v\ sobě ukrývá příjemnou vlastnost, kterou je\ nutnost použití další vrstvy vyrovnávacích pamětí a\ s\ tím spojenou režii ve\ formě kopírování bloků dat z\ a\ do\ těchto pamětí. Tato režie se\ navíc vyskytuje i\ na\ straně odesílatele -- zde ovšem z\ důvodu garantování zaslání nepoškozených dat. Každý odeslaný blok dat musí být na\ straně odesílatele uchován tak dlouho, dokud neobdrží potvrzení o\ přijetí.

Při zamyšlením nad zapojením komunikační vrstvy do\ nástroje DIVINE bychom narazili na\ další problém, kterým je\ paralelní přístup k\ přichozím zprávám. Na\ jednom výpočetním stroji lze mít na\ jednom portu otevřenou pouze jednu komunikačním linku, což znamená mít buď jedno dedikované vlákno na\ veškerou obsluhu spojení, nebo implementovat synchronizaci pomocí zámků pro\ paralelní přístup k\ socketu.

Spolu s\ nutností použít další vrstvu vyrovnávacích pamětí se\ pak dedikované vlákno jeví jako snadnější volba, neboť by bylo potřeba vhodně zvolit granularitu zámků pro paralelní přístup -- od\ jednoho globálního zámku, který je\ nevhodný z\ důvodu malé rychlosti a\ ztráty výhod paralelizmu, až\ po\ paralelního zámku pro každý blok paměti a\ socket, kde hrozí problémy typu uváznutí či\ porušení dat.

### Sekvenční sockety

Sekvenční sockety zde uvádím jen pro ilustraci. Jejich použití totiž nepřipadá v\ úvahu, neboť nejsou definovány pro síťovou komunikační vrstvu. Stále ale stojí za\ zmínku některé jejich vlastnosti.

#### Garance

Sekvenční sockety poskytují stejné garance jako spojité sockety. Vzhledem k\ rozborům uvedeným výše se\ jedná o\ klíčovou vlastnost,kterou nástroj DIVINE od komunikační vrstvy požaduje. Tudíž by\ byly z\ toho pohledu vhodnými kandidáty.

#### Explicitní stanovení hranic zpráv

Oproti spojitým socketům pevně ohraničují zprávy, takže se nemůže stát, že\ následkem chybného zpracování dojde k\ posunutí hranice konce zprávy. Nicméně tato vlastnost není klíčovou pro komunikační vrstvu -- jednalo by\ se\ pouze o\ přidání dalšího kontrolního mechanizmu.

### Zvolené řešení

Na\ výběr zůstaly spojité sockety implementované pomocí TCP a\ nespojité sockety, které jsou implementované pomocí UDP. Obě dvě implementace jsou jako standardní síťové protokolyy na\ strojích typicky implementovány na\ úrovni operačního systému. V\ případě TCP to\ přináší několik výhod:

1.  Veškerá obsluha spojení, příchozích dat a\ režie je\ prováděna v\ rámci jedné úrovně bezpečnosti procesoru [[Paul A. Karger, Andrew J. Herbert, An Augmented Capability Architecture to Support Lattice Security and Traceability of Access, sp, p. 2, 1984 IEEE Symposium on Security and Privacy, 1984]](http://www.computer.org/csdl/proceedings/sp/1984/0532/00/06234805-abs.html) a\ nedochází tak k\ vícenásobnému přepínání kontextu z\ uživatelského prostoru (*ring 3*) do\ jádra operačního systému (*ring 0*).
2.  Obsluha síťového rozhraní musí používat zámky pro paralelní přístup. Výhodou je, že\ u\ rozšířeného operačního systému, jakým GNU/Linux je, se\ očekává, že\ implementace nebude obsahovat problémy typu uváznutí a\ bude optimalizovaná.
3.  Implementace TCP pracuje šetrně s\ pamětí. Znamená to, že\ obsahy příchozích paketů skládá do\ vyrovnávacích pamětí a\ až\ v\ případě, že\ přijde z\ uživatelského prostoru požadavek na\ přijetí dat, se\ zpráva zkopíruje do\ programem označené paměti. Příchozí zpráva je\ tedy dvakrát kopírována.
4.  TCP je\ v\ provozu již více jak 30 let. Lze tedy očekávat, že\ případné chyby v\ protokolu a\ v\ implementacích operačních systémů již byly nalezeny a\ opraveny.

Oproti tomu použití nespojitých socketů umožňuje využít malou režii a\ zároveň vyšší rychlost díky kratším hlavičkám UDP paketů. Nevýhodou je\ nutnost implementovat vlastní protokol pro udržování spojení, pro garanci korektnosti dat a\ pro další dříve zmíněné požadované vlastnosti. Tento vlastní protokol navíc obrací výhodu uvedené u\ TCP na\ nevýhody:

1.  Obsluha spojení, příchozích dat a\ režije je\ prováděna v\ uživatelském prostoru. Každý dotaz na\ příchozí data nutně vyžaduje přepnutí kontextu z\ uživatelského do\ jádra opračního systému, což je\ drahá operace vzhledem k\ času [[X]](http://os.itec.kit.edu/65_1029.php).
2.  V\ případě paralelního přístupu by\ bylo potřeba navíc použít zámky pro paralelní přístup v\ uživatelském prostoru. Další zámky mohou hypoteticky snížit rychlost zpracování dat. V\ případě použití dedikovaného vlákna pro zpracování příchozích spojení pak musíme řešit navíc mezivláknovou komunikaci, kde se\ pravděpodobně také použijí zámky. Povšimněte si, že\ k\ zamykání systémových zámků dochází i\ při tomto přístupu.
3.  Další vrstva vyrovnávacích pamětí, kterou je\ potřeba použít, způsobí, že\ bloky dat nebudou před přijetím kopírovány dvakrát, ale třikrát.
4.  U čerstvě vytvořeného protokolu hrozí vyšší riziko chyb -- jak návrhové chyby, tak implementační.

**[obrázek práce s pamětí - TCP vs UDP+buffer]**

Z\ tohoto shrnutí se\ mi jeví jako vhodné použít spojité sockety spolu s\ TCP. Druhá možnost by\ znamenala dle mého názoru vyvýjet již vynalezenou věc znovu s\ nejistým výsledkem, zda bude rychlejší (a\ zda bude korektní).

## Architektura

Stávající řešení, kdy se\ MPI agent[^mpi-agent] postupně přes `ssh` [[RFC4252]](https://tools.ietf.org/html/rfc4252) připojí ke\ všem strojům, kteří se\ mají účastnit distribuovaného výpočtu, provede se\ výpočet a\ jednotlivé instance na\ výpočetních strojích se\ ukončí, vypovídá o\ pohledu, jakým se\ díváme na\ nástroj DIVINE. Jako na\ program, který jednou spustíme, on\ nám dá výsledek a\ tím je\ vše hotovo.

[^mpi-agent]: Jedná se\ o\ program, který spustí MPI aplikaci. Většinou se\ tento agent jmenuje `mpirun` nebo `mpiexec`.

Na\ nástroj DIVINE se\ ale můžeme dívat i\ jinak. Jako na\ službu, která běží někde ve výpočetním clusteru a\ která je\ dostupná skrz nějaké rozhraní -- skrz grafickou aplikaci, příkazový řádek či\ webovou stránku. Pro tento nový pohled je\ ale potřeba provést změny v\ architektuře. Jedná se\ zejména o\ rozdělení programu na\ dvě části. První a\ podstatnější je\ serverová část, která je\ zodpovědná za\ samotné provedení výpočtu. Druhou částí je\ klientská část, která je\ v\ aktuálním návrhu zodpovědná za\ zadání práce vybraným serverům.

### Server

Server bude mít jeden hlavní proces, jehož primárním úkolem je\ příjem příchozích spojení a\ odpovídání na\ požadavky. V\ případě započetí výpočtu dojde k\ vytvoření dalšího procesu, který je\ zodpovědný za\ provedení přiděleného výpočtu -- tzv. výkonný proces. Tím hlavní proces změní svoji roli a\ stává se\ opatrovníkem výkonného procesu. V\ jeden okamžik může běžet nanejvýš jeden výkonný proces, neboť nástroj DIVINE má velké nároky na\ paměť i\ na\ procesorový čas.

Primárním důvodem tohoto rozdělení je\ mít kontrolu nad\ kteroukoliv běžící instancí programu na\ vzdálených strojích. Tato funkcionalita je obzvláště vhodná v\ případě, že\ se\ distribuovaný výpočet zacyklí či\ dojde k\ uváznutí. Skrze hlavní proces lze pomocí funkce [`kill`](http://pubs.opengroup.org/onlinepubs/9699919799/functions/kill.html) výpočet výkonného procesu kdykoliv násilně ukončit. Dalším důvodem pak je\ zachování funkčního serveru i\ v\ případě, že\ dojde k\ fatální chybě a\ výkonný proces je\ ukončen operačním systémem.

Při spuštění nástroje DIVINE jako server se\ provedou kroky vedoucí k\ démonizaci. To\ je\ stav, kdy program sice běží pod uživatelem, který ho\ spustil, ovšem uživatel nemusí být fyzicky připojen k\ danému stroji. V\ tomto stavu server vyčkává, dokud nepřijde nějaké spojení. Během čekání hlavní proces nespotřebovává žádný procesorový čas, pouze malé množství paměti potřebné k\ běhu. Stejná situace -- žádná spotřeba procesorového času a\ pouze málo paměti -- je i\ v\ případě, že\ hlavní proces dozoruje běh výkonného procesu.

Výkonný proces poskytuje distribuovanému algoritmu možnost poslat zprávu nějakému stroji, přijmout zprávu od\ specifického stroje a\ poslat zprávu všem strojům. Pro větší podporu paralelního přístupu ke\ komunikačnímu rozhraní výkonný proces umožňuje mít víc kanálů mezi dvěma servery. Přijímat zprávy tak lze ze\ všech kanálů, či\ lze specifikovat jeden kanál, na\ kterém bude vlákno distribuovaného algoritmu přijímat zprávy.

Je\ pravidlem, že\ výkonné procesy jsou navzájem propojeny stejným počtem kanálů, přičemž primární účel kanálů je\ přepravovat data distribuovaného algoritmu. Mimo to\ je\ mezi každým výkonným procesem udržováno jedno řídicí spojení. Výkonný proces je\ po\ čas výpočtu spojeno s\ klientem jedním řídicím spojením, žádné datové kanály mezi nimi nejsou otevřeny.

Pro zachování sémantiky výstupních operací s\ MPI je\ server koncipován tak, aby zachytával jakékoliv pokusy o\ zápis na\ výstup a\ přeposílal je klientovi, který je\ zobrazí. Toho je\ ve\ výkonném procesu dosaženo přesměrováním standardního výstupu a\ standardního chybového výstupu a\ nastartováním dvou dalších vláken -- jeden pro každý výstup -- které se\ starají o\ samotné přeposílání. Více je popsáno v podkapitole Protokol.

Pro zjednodušení ladění chyb při vytváření nové implementace komunikační vrstvy, kdy se\ server fyzicky nachází na\ jiném stroji než spuštěný klient, jsem přidal do\ serveru možnost logovat jednotlivé prováděné operace. V\ demonstračním programu se\ jedná především o\ záznamy vstupů do\ funkcí, které obsluhují reakce na\ příkazy, a\ záznamy problematických situací (popsáno později).

### Klient

Klient slouží k\ ovládání výpočetních serverů a\ nikterak se\ přímo nepodílí na\ výpočtu. Mezi základní funkce patří spuštění démonů na\ určených strojích, jejich ukončení, restart, dotázání se\ na\ aktuální stav (zda je\ server volný či\ zda na\ něm probíhá výpočet) a\ pak samozřejmě spuštění samotného výpočtu.

Spuštění démonů na\ určených strojích je\ aktuálně provedeno pomocí `ssh` připojení, po\ kterém následuje spuštění stejného programu jako je\ klient, ale v\ režimu serveru. Toto řešení má několik problémů, je\ například vázané na\ operační systém GNU/Linux (z\ důvodu nalezení cesty ke\ spuštěnému klientovi).

Spuštění samotného výpočtu probíhá v\ několika krocích. Nejprve je\ potřeba ověřit, zda jsou všechny strojích běží server a\ že\ je server dostupný -- tj. neběží na něm aktuálně výpočet. Po\ vytvoření spojení ke\ každému stroji klient postupně požádá všechny servery, aby se\ spojily s\ ostatními na\ určeném počtu kanálů a\ vytvořily tak úplný graf. Následně klient může, pokud je\ to potřeba, doručit každému serveru blok dat, například obsah souboru. Poté nastává poslední fáze, která spočívá v\ přenesení argumentů z\ příkazové řádky a\ spuštění samotného výpočtu.

V\ průběhu výpočtu klient vyčkává na\ příchozí zprávy, které mohou být několika druhů. Mohou to\ být zachycené výstupy, které klient vypíše do\ správného výstupního proudu. Mohou to být hromadné zprávy, či\ chybně zaslané zprávy na\ klienta, které jsou ignorovány. V\ neposlední řadě se\ může jednat o\ zprávu, že\ ten který server dokončil výpočet.

Pro spuštění poslední fáze, rozpojení sítě, je\ potřeba, aby všechny servery zaslaly zprávu o\ ukončení výpočtu. Pak klient rozpustí síť serverů, což zapříčiní ukončení výkonných procesů a\ hlavní procesy se\ uvedou do\ původního stavu. Následně i\ klient ukončí svoji činnost.

## Protokol

Pro komunikaci jsem zavedl jednoduchý protokol postavený na\ zprávách. Zprávy se\ posílají v\ binárním formátu, takže se\ vyžaduje, aby všechny servery i\ klient měli stejnou endianitu. To\ může být chápáno jako případné problematické místo, ovšem neočekávám, že\ bude nástroj DIVINE spouštěn na\ hybridních výpočetních clusterech. K\ binárnímu formátu jsem přistoupil také kvůli rychlosti serializace.

Dále v\ popisu protokolu mluvím o\ seznamu strojů. Jedná se\ o\ seznam, který obsahuje síťové adresy strojů v\ lokální síti a\ který zároveň definuje stroje, které se\ budou podílet na\ distribuovaném výpočtu. Při adresaci není uvedené číslo portu, neboť to\ se\ nastavuje při startu klienta a\ musí být stejné s\ číslem portu, na\ kterých poslouchají servery uvedené v\ seznamu strojů.

### Formát zpráv

Každá zpráva má\ hlavičku proměnlivé délky, která obsahuje následující informace:

*   kategorie
*   počet fragmentů
*   rank odesílatele
*   rank příjemce
*   štítek
*   velikosti fragmentů (více políček)

Políčko *kategorie* slouží k\ rozeznání typů zpráv. Jinou hodnotu mají řídicí zprávy, které jsou součástí protokolu a\ jsou obsluhovány serverem nebo klientem, další hodnotu mají zprávy přeposílající obsah na\ standardní výstup a\ standardní chybový výstup a\ konečně datové zprávy, za\ jejichž zpracování je zodpovědný distribuovaný algoritmus. Políčko *kategorie* je 8bitové neznaménkové číslo -- typ `uint8_t`.

Políčko *počet fragmentů* udává kolik datových fragmentů má\ zpráva. Toto políčko je 8bitové neznaménkové číslo -- typ `uint8_t`.

*Rank odesílatele* je číslo serveru v\ rámci ustanovené sítě. Toto políčko je 8bitové neznaménkové číslo -- typ `uint8_t`.

*Rank příjemce* je\ číslo serveru v\ rámci ustanovené sítě. Obsahuje buď konkrétní číslo, nebo konstantu `255`, která značí, že\ zpráva byla zaslána hromadně. Toto políčko je 8bitové neznaménkové číslo -- typ `uint8_t`.

*Štítek* slouží k\ určení vlastnosti předávané zprávy. V\ případě datové zprávy je\ hodnota *štítku* plně v\ režii distribuovaného algoritmu. Protokol je\ používá k\ označení příkazů a\ odpovědí a\ v\ případě přeposílání obsahu na\ výstup se\ jím rozlišuje standardní výstup od\ standardního chybového výstupu. Toto políčko je 32bitové neznaménkové číslo -- typ `uint32_t`.

*Velikosti fragmentů* je\ pole 32bitových čísel, které reprezentuje velikosti jednotlivých datových fragmentů zprávy. Počet fragmentů, tedy velikost pole *velikosti fragmentů* určuje hodnota *počet fragmentů*.

Zpráva poskytuje metody pro manipulaci s\ hlavičkou zprávy a\ pro přidávání a\ čtení datových fragmentů do/ze\ zprávy. Pro\ omezení množství kopírovaných dat umožňuje zpráva při příjmu dat rovnou alokovat paměť pro\ datový fragment.

### Seznam příkazů

Seznam příkazů, odpovědí a\ oznámení, které se\ používají v\ navrženém protokolu, je\ následující:

    OK, Refuse, Enslave, Disconnect, Peers, ConnectTo, Join, DataLine,
    Grouped, InitialData, Run, Start, Done, PrepareToLeave, Leave,
    CutRope, Error, Renegade, Status, Shutdown, ForceShutdown, ForceReset

Každý prvek z\ tohoto výčtu má\ svoji číselnou hodnotu, která je\ přiřazena do\ *štítku* zprávy.

Odpověď `OK` je\ akceptující odpovědí na\ všechny příkazy. Oproti tomu je\ odpověď `Refuse` zamítajícím stanoviskem.

Příkaz `Enslave` slouží k\ zotročení serveru klientem. Má tři parametry: *rank stroje*, *síťové jméno stroje* a *počet kanálů*. *Rank stroje* postupně nabývá hodnot v\ rozsahu 1 až\ počet strojů. Oproti protokolu MPI zde platí, že\ jsou čísla o\ jedno posunuté; rank\ 0 je\ vyhrazen klientovi. *Síťové jméno stroje* si\ server uloží do\ tabulky spojení, přičemž jméno je\ případně posléze použito pro hlášení chyb. *Počet kanálů* označuje, kolik datových kanálů bude otevřeno mezi každou dvojcí strojů.

Příkaz `Disconnect` použije klient v\ případě, že\ hodlá ukončit spojení se\ serverem.

Příkaz `Peers` má jeden parametr: *velikost světa*. Ten označuje počet strojů ve\ skupině. Příkaz zasílá klient serverům.

Příkazem `ConnectTo` klient požádá server **A** o\ vytvoření spojení s\ dalším serverem **B**. Příkaz má tři parametry: *rank stroje*, *síťové jméno stroje* a\ *adresu*. První dva parametry jsou stejné jako u\ předchozího příkazu. *Adresu* použije server **A** pro připojení k\ serveru **B**.

Příkaz `Join` použije server **A** pro otevření řídicího spojení k\ serveru **B**. Má dva parametry: *rank* serveru **A** a\ *síťové jméno* serveru **A**.

Příkaz `DataLine` použije server **A** pro otevření datového komunikačního kanálu k\ serveru **B**. Má dva parametry: *rank* serveru **A** a\ *číslo kanálu*.

Příkaz `Grouped` posílá klient serverům v\ okamžiku dokončení propojování serverů.

Příkaz `InitialData` je volitelný a\ klient ho\ posílá spolu s\ blokem počátečních dat všem serverům.

Příkazem `Run` pobídne klient servery, aby spustili distribuovaný výpočet. Jako parametry jsou argumenty, které budou předány spouštěné funkci.

Příkaz `Start` slouží jako bariéra. Po\ přijetí server spouští distribuovaný výpočet.

Oznámení `Done` posílají servery klientovi po\ dokončení distribuovaného algoritmu.

Příkazem `PrepareToLeave` klient připraví výkonný proces na\ ukončení činnosti.

Oznámením `CutRope` dává na\ serveru výkonný proces najevo hlavnímu procesu, že\ ukončuje svoji činnost.

Po obdržení příkazu `Leave` ukončí výkonný proces svoji činnost.

Příkazy `Error` a\ `Renegade` slouží k\ oznámení chyby všem propojeným strojům. Příkaz `Renegade` má jeden parametr: *adresu* stroje, kde došlo k\ chybě.

Příkazem `Status` se\ klient ptá na\ stav serveru a\ stejným příkazem mu\ server odpovídá.

Oba příkazy `Shutdown` a\ `ForceShutdown` slouží k\ ukončení činnosti serveru. `ForceShutdown` ukončí server vždy, `Shutdown` pouze tehdy, když se\ server neúčastní výpočtu.

Příkaz `ForceReset` slouží k\ násilné obnově výchozího stavu serveru.

### Stavy serveru

Server se\ může v\ průběhu svého běhu nacházet v\ jednom z\ následujích stavů:

1.  *Volný*. Server je\ k\ dispozici.
2.  *Zotročený*. Server byl osloven klientem a\ podřídil se\ mu.
3.  *Formující skupinu*. Server je ve\ fázi vytváření spojení ke\ všem ostatním serverům ve\ skupině.
4.  *Zformován*. Všechny servery jsou navzájem spojeny a\ došlo k\ vytvoření výkonného procesu.
5.  *Dohlížející*. Hlavní proces začal dohlížet na\ výkonný proces.
6.  *Běžící*. Výkonný server spustil distribuovaný algoritmus.
7.  *Ukončující*. Výkonný server započal ukončující fázi.

Některé stavy jsou vyhrazeny pouze pro hlavní proces (*volný*, *zotročený*, *formující skupinu*, *dohlížející*). Do zbývajících stavů se\ může dostat pouze výkonné vlákno. Pokud se\ klient dotáže serveru na\ to, v\ jakém je\ stavu, jako odpověď dostane jeden ze\ stavů hlavního procesu. Zjistit stav výkonného procesu není možné.

**[přechodový diagram stavů]**

Server může měnit svůj stav na\ základě příkazu od\ klienta. Na\ základě jakého příkazu, případně události, server změní svůj stav je\ popsáno na\ uvedeném přechodovém diagramu stavů. Dále zde platí to, že\ téměř všechny příkazy od\ klienta může server akceptovat, pouze pokud je\ ve\ správném stavu. Přechody mezi jednotlivými stavy jsou tak primárně kontrolní mechanismus.

### Ustanovení sítě

Jak již bylo lehce zmíněno, ustanovení sítě probíhá ve\ třech fázích: navázání spojení, propojení a\ spuštění. Na\ ilustračním obrázku je\ zakreslen průběh úspěšného ustanovení sítě. Jednotlivé fáze jsou pak rozepsány níže.

**[diagram ustanovení sítě]**

#### Navázání spojení

Klient postupně otevírá spojení na\ každý stroj ze\ seznamu strojů. Po každém úspěšném připojení klient zašle serveru příkaz `Enslave`, na\ nějž může server odpovědět třemi způsoby. Pokud je\ ve\ stavu *volný*, může příkaz akceptovat, čímž sám sebe přepne do\ stavu *zotročený*. Nebo může server zamítnout zotročení v\ případě, že se\ nachází v\ jiném stavu než *volný*. Pokud server odpoví jinak, je\ odpověď nahlášena jako chybná a\ řeší se\ jako chybový stav.

Pokud se\ klient není schopný k\ některému stroji připojit, případně dostane zamítavou odpověď, rozešle všem již připojeným serverům zprávu `Disconnect`, počká na\ odpověď a\ ukončí všechna spojení. Zpráva `Disconnect` způsobí, že\ server pošle akceptující odpověď, přejde zpět ze\ stavu *zotročený* do\ stavu *volný* a\ smaže všechny nabyté znalosti. V\ opačném případě -- podařilo se\ vytvořit spojení se\ všemi stroji -- pokračuje klient druhou fází: propojením.

#### Propojení

V\ této fázi klient rozešle všem zotročeným serverům příkaz `Peers` spolu s\ jedním parametrem velikosti světa. Přijetím tohoto příkazu se\ server přepne ze\ stavu *zotročený* do\ *formující skupinu* a\ zapamatuje si\ velikost světa.

Server neakceptuje příkaz `Peers` pouze v\ případě, že\ se\ nenachází ve\ stavu *zotročený*. Tato situace je\ řešena jako chybový stav, neboť k\ ní\ může dojít pouze chybou v\ protokolu samotném.

Po\ obeznámení všech serverů s\ velikostí světa posílá klient postupně jednotlivým serverům žádosti o\ spojení s\ dalšími servery, což provádí příkazem `ConnectTo`. Na\ základě žádosti požádaný server **A** postupně pomocí příkazů `Join` a\ `DataLine` ustanoví spojení s\ cílovým serverem **B**. Počet zaslání příkazů `DataLine` zavisí na\ počtu požadovaných otevřených datových kanálů. Pokud se\ podaří navázat všechna spojení, pošle server **A** klienovi odpověď `OK`, v\ opačném případě odpoví `Refuse`.

Pokud klient obdrží od\ některého serveru odpověď `Refuse`, řeší se\ nastalá situace jako chybový stav. Jinak klient zašle serverům příkaz `Grouped`, který způsobí, že\ na\ každém serveru hlavní proces vytvoří výkonný proces. Možnost přijímat příchozí spojení zůstane pouze hlavnímu procesu, naopak výkonný proces dostane na\ starost všechny již otevřená spojení. Výkonný a\ hlavní proces spolu zůstanou v\ kontaktu pomocí anonymního páru socketů vytvořených funkcí [`socketpair`](http://pubs.opengroup.org/onlinepubs/9699919799/functions/kill.html). Hlavní proces se\ přepne do\ stavu *dohlížející*, zatímco výkonný proces se\ přepne do\ stavu *zformován*. Od\ tohoto okamžiku klient komunikuje pouze s\ výkonným procesem.

#### Spuštění

Poslední fází k\ započetí distribuovaného algoritmu je\ spuštění. Před samotným spuštění může klient rozeslat počáteční data všem výkonným procesům pomocí příkazu `InitialData`. Po něm již následuje zaslání příkazu `Run` spolu s\ parametry příkazové řádky, kterým se\ výkonný proces přepne ze\ stavu *zformován* do\ stavu *běžící*.

Samotný start algoritmu v\ sobě implementuje bariéru, kdy je\ až\ příkazem `Start` od\ klienta každý výkonný proces zpraven o\ skutečném odstartování. Tento krok je\ v\ protokolu použit, aby nedocházelo na\ jednom stroji k\ započetí výpočtu, když se\ klient ještě stará o\ rozeslání dat zbylým strojům.

Před samotným startem distribuovaného algoritmu se\ navíc spustí dvě vlákna, která mají na\ starost přesměrování standardního výstupu a\ standardního chybového výstupu na\ klienta. Klient se\ tak od\ tohoto okamžiku stará pouze o\ zobrazování přeposlaných výstupů a\ případné řešení chybových stavů.

### Rozpuštění sítě

Rozpuštění sítě započne v\ okamžiku, kdy první stroj pošle oznámení `Done`, kterým informuje klienta o\ dokončení výpočtu. Až\ klient obdrží tato oznámení od\ všech strojů, přejde k\ samotnému ukončení. To\ zahrnuje rozeslání příkazu `PrepareToLeave` od\ klienta všem serverům, který způsobí, že\ se\ klient přepne ze\ stavu *běžící* do\ stavu *ukončující*. V\ tomto stavu přestává server hlásit případné výpadky spojení.

Jakmile dostane klient odpověď od\ všech výkonných procesů, že\ přešly do\ stavu *ukončující*, rozešle jim příkaz `Leave`. Ten způsobí, že\ každý výkonný proces pošle oznámení `CutRope` svému hlavnímu procesu, po\ jehož přijetí se\ hlavní proces na\ každém serveru přepne ze\ stavu *dohlížející* do\ stavu *volný*. Souběžně s\ tím dojde na\ straně každého výkonného procesu k\ uzavření všech spojení a\ k\ ukončení činnosti.

Na\ komunikaci mezi výkonným a\ dohlížejícím procesem je\ klíčové, aby si\ výkonný proces počkal na\ potvrzení přijetí oznámení `CutRope` od\ hlavního procesu. Vynecháním potvrzení totiž může docházet k\ situaci, kdy klient již ukončil svoji činnost, ale hlavní proces je\ stále ve\ stavu *dohlížející*, což může mít za\ následek, že\ další spuštění výpočtu za\ použití stejných serverů nebude možné provést.

### Ostatní příkazy

Klient může operovat s\ dalšími příkazy: `Status`, `Shutdown` a `ForceShutdown`.

Pomocí dotazu `Status` může klient zjistit, v\ jakém stavu se\ nachází server. Běžně je\ pro zjištění stavu vytvořeno nové spojení na\ server. Ten po\ obdržení dotazu na\ svůj stav pošle zprávu s\ textovým popisem svého stavu. Server se\ nikterak nestará o\ nové spojení, které tím pádem zaniká. Server na\ dotaz na\ svůj stav odpoví vždy, ať\ už\ je v\ jakémkoliv stavu. Protože se\ ale klient dotazuje pouze hlavního procesu, může dostat jako odpověď pouze tyto stavy: *volný*, *zotročený*, *formující skupinu* a\ *dohlížející*.

Příkaz `Shutdown` je\ zdvořilou žádostí o\ ukončení běhu serveru. Serveru tuto žádost akceptuje pouze v\ případě, že\ se\ nachází ve\ stavu *volný*.

Příkaz `ForceShutdown` je\ silnější variantou předchozího příkazu, která zaručí, že\ server ukončí svoji činnost nezávisle na\ stavu, ve\ kterém se\ nachází. Navázaná spojení jsou ukončena bez zaslání jakékoliv zprávy a\ v\ případě, že\ je nastartován výkonný proces, je\ násilně ukončen.

Poslední příkaz -- `ForceReset` -- slouží k\ tomu, aby hlavní proces přešel do\ stavu *volný*, ať\ už\ byl předtím v\ kterémkoliv jiném stavu, což zahrnuje také násilné ukončení výkonného procesu, pokud je\ spuštěn. Tento příkaz kromě explicitního vyžádání od\ uživatele použije klient v\ případě, že\ obdržel [signál](http://pubs.opengroup.org/onlinepubs/9699919799/functions/V2_chap02.html#tag_15_04), který by\ způsobil ukočení klienta. Zejména se\ jedná o\ [signály](http://pubs.opengroup.org/onlinepubs/9699919799/basedefs/signal.h.html) `SIGALRM`, `SIGINT`[^SIGINT] a\ `SIGTERM`.

[^SIGINT]: Tento signál je\ na unixových systémech většinou generován prostřednictvím klávesové zkratky `Ctrl+C`.

### Řešení chyb a problémových stavů

Mnou navržený protokol je\ připraven řešit některé problémové situace, které mohou nastat. Problémové situace lze rozdělit do\ několika okruhů: selhání síťových komponent, vnitřní chyba protokolu a\ problém s\ distribuovaným algoritmem.

Mnohé z\ potenciálně problematických operací jsou ošetřeny nastavením maximálního časového úseku, po\ který mohou operace blokovat provádění. Pokud ani po\ uplynutí časového úseku nebylo možné operaci dokončit, dojde ve\ většině případů k\ vyhození výjimky `WouldBlock`.

Jakákoliv vyhozená výjimka je, pokud toto chování distribuovaný algoritmus neupraví, zachycena v\ nejvrchnější funkci, je\ reportována uživateli -- klient vypíše chybovou hlášku na\ obrazovku, server ji\ zaznamená do\ logů -- a\ program je\ ukončen. Odlišné chování má\ akorát hlavní serverový proces, který sice zachycenou výjimku zaznamená do\ logů, ovšem neukončí svoji činnost, ale uvolní veškeré alokované zdroje, případně násilně ukončí běh výkonného procesu, a\ přejde do\ stavu *volný*. Tento mechanizmus zpracování výjimek jsem zvolil, aby při nastalých problémech nedocházelo k\ uváznutí, ale aby byl naopak uživatel zpraven o\ nastalé chybě.

Za\ chyby se\ v\ okruhu selhání síťových komponent považuje cokoliv od\ zamítnutí připojení až\ po\ rozpojení sítě během výpočtu. V\ mé implementaci protokolu většinu těchto problémů řeší objektová nadstavba nad POSIX rozhraním BSD socketů.

Problémy vzniklé během vytváření spojení mezi dvěma stroji mohou mít několik příčin. Pokud se\ nepodaří přeložit název cílového stroje na\ IP adresu, ať již z\ důvodu nedostupnosti stroje, či\ protože je\ v\ názvu překlep, dojde k\ vyhození výjimky s\ relevantním popiskem. Jiná výjimka může být vyhozena, pokud se\ do\ určitého časového okamžiku nepodaří spojení navázat.

Po\ úspěšném navázání spojení je\ každý nově otevřený socket poznačen jako blokující[^blocking-socket] a\ zároveň mu je\ nastaveno, jak nejdéle může trvat operace nad socketem. Časový limit v\ tomto případě ohraničuje především operace čtení a\ zápisu do\ socketu. Operace zápisu se\ může zdržet například proto, že\ cílový stroj si\ nevyzvedává příchozí zprávy a\ vyrovnávací paměť operačního systému pro příchozí data je\ již plná. U\ operace čtení může naopak dojít k\ tomu, že\ zdrojový stroj nepošle žádná data. Tímto se\ řeší především situace, kdy dojde k\ uváznutí na\ některém serveru z\ důvodu chybného distribuovaného algoritmu. Dalším problémovým místem pak může být ztráta spojení mezi dvěma stroji v\ průběhu výpočtu. Tato situace je\ opět zachycena objektovou nadstavbou, jejíž reakce je\ vyhození výjimky `ConnectionAbortedException`.


[^blocking-socket]: Každý získaný socket je blokující. Explicitně tuto informaci uvádím, protože během připojení jsou sockety z\ důvodu prevence uváznutí označeny jako neblokující a\ ke změně na\ blokující dojde až\ po vytvoření spojení.

Pro eliminaci chyb v\ samotném protokolu jsem přistoupil jednak k\ použití stavů u\ serverových procesů, jednak k\ posílání odpovědí na\ každý příkaz -- vyjma příkazů `ForceShutdown` a\ `ForceReset`. Server odmítne vykonat příkaz, pokud pro jeho splnění nejsou vhodné podmínky, například protože se\ jeho procesy nachází ve\ špatných stavech.

Reakce na\ odmítnutí příkazu se\ může lišit dle závažnosti. Některá odmítnutí nemusí být závažná a\ mohou být ignorována či\ pouze ohlášena uživateli (forma závisí na\ tom, zda se\ jedná o\ klienta či server), některá ovšem mohou způsobit vyhození výjimky. Většina zamítnutí ovšem vyústí ve\ vyhození výjimky.

Nevhodný návrh distribuovaného algoritmu spolu s\ chybnou implementací mohou vyústit v\ takové uváznutí, kdy některý výkonný proces nebude pracovat s\ otevřenými spojeními, což může i\ v\ případě ukončení klienta vést k\ tomu, že\ tento výkonný proces nebude ani po\ násilném rozpuštění sítě schopen zastavit výpočet. Z\ toho důvodu je\ v\ rámci klienta použito zachytávání některých signálů (zejména `SIGINT` a\ `SIGTERM`), které způsobí, že\ klient zašle všem uváznutým serverům (konkrétně hlavním procesům) příkaz `ForceReset`.

### Bezpečnost

Protokol jako takový byl koncipován na\ provoz v\ bezpečném prostředí, takže nepodporuje žádnou úroveň zabezpečení. Ze\ základních prvků zabezpečení zde chybí hlavně autentizace klienta vůči serveru a\ kontrola integrity dat.

Žádná metoda zabezpečení nebyla nasazena ze\ dvou důvodů. První z\ nich byl již vyřčen -- očekávám, že\ nástroj DIVINE bude provozován v\ zabezpečené vnitřní síti. Druhým důvodem je\ velký důraz na\ rychlost. Ačkoliv by\ nasazení autentizace sice nešlo proti druhému důvodu, implementace integrity dat například pomocí SSL [[RFC6101]](https://tools.ietf.org/html/rfc6101) by\ již mohla způsobit snížení rychlosti toku dat. Protože však záměr práce nespočívá v\ nasazení zabezpečovacích prvků, je\ dle mého soudu zbytečné tyto mechanizmy do\ protokolu implementovat.

## Rozhraní pro distribuované procházení grafu

Při návrhu rozhraní jsem vycházel převážně z\ požadavků nástroje DIVINE. Všechny aktuálně implementované distribuované algoritmy využívají z\ knihovny MPI pouze několik málo funkcí pro komunikaci. Funkce `MPI_Send`/`MPI_Isend` , `MPI_Probe`/`MPI_Iprobe` a\ `MPI_Recv`/`MPI_Irecv`. Ačkoliv některé algoritmy navíc zasílají zprávu všem spolupracujícím instancím, k\ tomu se\ ale nevyužívá funkce `MPI_Bcast`, nýbrž opakované volání funkce pro zaslání zprávy.

Dalším požadavkem bylo, aby šla komunikační vrstva použít v\ paralelním kontextu bez nutnosti řešit zamykání uvnitř algoritmu a\ tím lépe využít paralelizmu. Přestože jsem neměl k\ dispozici kompletní požadavky na\ rozhraní z\ důvodu souběžně vyvíjené nové verze nástroje DIVINE, snažil jsem se\ co\ nejvíc vyhovět požadavkům, které jsem dostal. Způsob komunikace -- zasílání zpráv -- zůstává nadále platný i\ pro novou verzi.

### Zprávy

Zprávy jsou v\ rozhraní reprezentovány dvěma třídami. Třída `InputMessage` reprezentuje příchozí zprávu, třída `OutputMessage` pak odchozí zprávu. Důvod pro dvě odlišné třídy je\ daný jiným očekávaným chováním každé z\ nich, především co\ se\ práce s\ pamětí týká.

Do\ odchozí zprávy je možné přidat data buď pomocí metody `add` nebo pomocí operátoru `<<`. Druhý způsob umožňuje připojit ke\ zprávě i\ třídu `std::string` případně `std::vector`. Pro data připojené pomocí operátoru `<<` navíc platí, že\ typ dat musí splňovat koncept [`TriviallyCopyable`](http://en.cppreference.com/w/cpp/concept/TriviallyCopyable).

Připojení konstantních dat způsobí vytvoření jejich kopie. K\ tomuto kroku jsem přistoupil, protože použitá funkce z\ rozhraní BSD socketů (`sendmsg`) neumožňuje předat ukazatel na\ konstantní data. Vzhledem ke\ stávajícímu způsobu práce s\ pamětí v\ nástroji DIVINE očekávám, že\ i\ nadále bude výskyt konstantních dat sporadický. Co\ je\ ovšem důležitější, je platnost nekonstantních dat, kdy tato data musí být validní dokud nedojde k\ odeslání zprávy.

Příchozí zpráva také poskytuje více nástrojů pro práci s\ pamětí. Jednou možností je\ použít operátor `>>` a\ nastavit tak, do\ které proměnné se\ má nastavit fragment dat. V\ případě použití této možnosti je\ potřeba zaručit, aby nastavný počet a\ velikosti fragmentů odpovídaly fragmentům skutečně přijatým; v\ opačném případě dojde k\ vyhození výjimky.

Druhou možností je\ nastavit při příjmu vlastní funkci, která bude alokovat paměť postupně pro každý datový fragment, čímž se\ programátor stává zodpovědný za\ to, že\ alokovaná paměť bude korektně uvolněna.

### Služby komunikačního rozhraní

Ačkoliv lze chápat služby komunikačního rozhraní jako funkce, případně jako statické metody, implementoval jsem je\ z\ důvodů přehlednosti jako metody třídy `Daemon`, případně předka `Communicator`. Tato třída má vytvořenou jednu globální instanci, která je dostupná krze statickou metodu `instance`.

Rozhraní poskytuje tyto několik metod, které by\ bylo možné rozdělit na\ dvě kategorie: komunikační a\ servisní. Komunikační, jak již název evokuje, slouží k\ zasílání a\ přijímání zpráv. Servisní sdružují metody, pomocí kterých lze získávat informace o\ prostředí.

#### Servisní metody

`int Communicator::rank() const;`

:   \ \
    Metoda vrací rank procesu.

`bool Daemon::master() const;`

:   \ \
    Metoda vrací `true`, pokud je\ rank procesu roven 1.

`int Communicator::worldSize() const;`

:   \ \
    Metoda vrací počet procesů ve\ skupině.

`int Communicator::channels() const;`

:   \ \
    Metoda vrací počet dostupných datových kanálů mezi procesy.

`const std::string &Communicator::name() const;`

:   \ \
    Metoda vrací název stroje, na\ kterém běží proces.

`char *Daemon::data() const;`

:   \ \
    Metoda vrací ukazatel na\ počáteční data.

`size_t Daemon::dataSize() const;`

:   \ \
    Metoda vrací velikost počátečních dat v\ bytech.

`void Daemon::exit(int returnCode);`

:   \ \
    Metoda zahlásí vedoucímu procesu ukončení činnosti a\ následně program. Parametr *returnCode* slouží jako návratový kód procesu. Slouží jako náhrada za\ knihovní funkci `exit`, která ovšem neukončí korektně komunikaci s\ ostatním procesy.

    Metoda nesmí být volána v\ průběhu provádění metody `probe`, pokud je\ metoda `probe` volána nad\ hlavním komunikačním kanálem, neboť by mohlo dojít k\ uváznutí.

#### Komunikační metody

Většina komunikačních metod může při problémech v\ síti vyhazovat různé výjimky. Pro všechny výjimky platí, že\ jsou třídami odvezenými ze\ třídy `brick::net::NetException`, která je\ sama odvozena ze\ třídy `std::exception`.

\defmultiline{bool Daemon::sendTo(int rank, OutputMessage \&msg, ChannelID chID);\\ bool Daemon::sendTo(Channel channel, OutputMessage \&msg);}

:   \ \
    Základní metody sloužící k\ zaslání zprávy *msg*. První z\ uvedených funkcí přeloží *rank* a\ *chID* na\ kanál *channel*, kterým se pošle zpráva, a\ zavolá se\ druhá uvedená metoda. Pokud není parametr *chID* uveden, použije se\ řídicí kanál. Sémantika odeslání je\ vzhledem k\ použití systémového volání `sendmsg` blokující bafrovaná operace.

    Funkce vrací `true`, pokud byla operace úspěšně provedena. Vrací `false` v\ případě chybného parametru *rank* nebo *chID* -- neexistující rank nebo neexistující kanál. Metody mohou vyhodit tyto výjimky:

    *   `WouldBlock` -- Použitý socket má příznak neblokující a\ očekávaný přenos by\ způsobil blokování, případně došlo k\ dosažení časového limitu odesílací operace. Proměnná `errno` má hodnotu `EAGAIN` nebo `EWOULDBLOCK`.
    *   `ConnectionAbortedException` -- Došlo k\ chybě ve\ spojení. Proměnná `errno` má hodnotu `EPIPE`, `ENOTCONN`, nebo `ECONNREFUSED`.
    *   `SystemException` -- Došlo k\ chybě při přenosu a\ hodnota proměnné `errno` je\ odlišná od\ již uvedených.
    *   `DataTransferException` -- Došlo sice k\ úspěšnému přenosu, ale byla přenesena pouze část zprávy.

\defmultiline{bool Daemon::receive(int rank, InputMessage \&msg, ChannelID chID);\\bool Daemon::receive(Channel channel, InputMessage \&msg);}

:   \ \
    Základní metody sloužící k\ příjmu zprávy *msg* z\ konkrétního komunikačního kanálu. Obdobně jako u\ metod u\ první `sendTo` zde platí, že\ parametry *rank* a\ *chID* se\ přeloží na\ kanál *channel* a\ použije se\ druhá uvedená metoda `receive`. Pokud není parametr *chID* uveden, použije se\ řídicí kanál. Sémantika přijetí zprávy je\ blokující nebafrovaná operace.

    Funkce vrací `true`, pokud byla operace úspěšně provedena. Vrací `false` v\ případě chybného parametru *rank* nebo *chID* -- neexistující rank nebo neexistující kanál. Metody mohou vyhodit tyto výjimky:

    *   `WouldBlock` -- Použitý socket má příznak neblokující a\ očekávaný přenos by\ způsobil blokování, případně došlo k\ dosažení časového limitu přijímací operace. Proměnná `errno` má hodnotu `EAGAIN` nebo `EWOULDBLOCK`.
    *   `ConnectionAbortedException` -- Došlo k\ chybě ve\ spojení. Proměnná `errno` má hodnotu `ENOTCONN`, nebo `ECONNREFUSED`.
    *   `SystemException` -- Došlo k\ chybě při přenosu a\ hodnota proměnné `errno` je\ odlišná od\ již uvedených.
    *   `DataTransferException` -- Došlo sice k\ úspěšnému přenosu, ale byla přijata pouze část zprávy.

`bool Daemon::sendAll(OutputMessage &msg, ChannelID chID);`

:   \ \
    Metoda sloužící k\ zaslání dané zprávy *msg* všem procesům ve\ skupině pomocí kanálu *chID*. Pokud se\ neuvede parametr *chID*, bude jako komunikační kanál zvolen řídicí kanál.

    Aktuální implementace této metody je\ taková, že\ dojde k\ vytvoření $N - 1$ vláken, které každé pošle zprávu jednomu procesu. Po skončení odesílání se\ zkontroluje, zda se\ podařilo zprávu poslat všem. Pokud ano,vrátí metoda hodnotu `true`, jinak `false`. Metoda nevyhazuje žádné výjimky.

    Vzhledem k\ tomu, že\ je\ metoda málo používaná, zvolil jsem řešení s\ paralelním odesíláním. Pokud by\ v\ budoucnu začala metoda být více používaná, bylo by\ vhodné změnit implementaci odesílání z\ paralelní na\ hyperkubickou [W. Danny Hillis (1986). The Connection Machine. MIT Press. ISBN 0262081571.].

`int Daemon::probe(Ap applicator, ChannelID chID, int timeout);`

:   \ \
    Metoda, která zkouší přijímat příchozí spojení od\ všech procesů na\ kanálu *chID*. Na\ kanálech, ze\ kterých lze přijímat zprávy, se\ postupně volá funkce *applicator*.

    Funkce *applicator* může mít dvojí signaturu:

    *   `void applicator(Channel channel);` -- Základní varianta funkce *applicator* může skrze parametr *channel* zpracovat příchozí zprávu, případně také skrze stejný kanál odpovědět.
    *   `bool applicator(Channel channel);` -- Rozšířená varianta funkce *applicator* může navíc v\ návratové hodnotě sdělit, jestli se\ má pokračovat v\ aplikování funkce *applicator* na\ kanály. Pro pokračování je\ třeba vrátit `true`, pro ukončení zpracování `false`.

    Parametr *timeout* udává ve\ vteřinách, jak dlouho má\ metoda `probe` čekat. Pokud je *timeout* roven $0$, nebude `probe` čekat vůbec, naopak pokud bude hodnota parametru $-1$, bude čekat, dokud nebude některé spojení připraveno na\ příjem zprávy. Implicitní hodnota parametru *timeout* je $-1$.

    Metoda `probe` vrací číselnou hodnotu kolikrát byla na\ příchozí spojení zavolána funkce *applicator*. Počet zpracovaných příchozích spojení se\ ale může lišit, neboť metoda `probe` zpracovává veškeré zprávy, ale jenom zprávy kategorie `Data` předává ke\ zpracování funkci *applicator*.

    Každý proces by\ měl v\ jednom vlákně pomocí metody `probe` pravidelně kontrolovat řídicí kanály, neboť uvnitř metody dochází k\ obsluze protokolu.

    **Poznámka:** Metoda `probe` by\ se měla volat periodicky v\ cyklu, neboť se\ může vrátit aniž by\ zavolala funkci *applicator* -- buď z\ důvodu vypršení časového limitu, nebo z\ důvodu zpracování zprávy jiné kategorie než datové.

# Experimentální porovnání

Nová implementace poskytuje nástroji DIVINE několik způsoby použití, z\ nichž jsem vybral dva relevantní scénáře. První z\ nich je, že\ bude v\ rámci každého procesu jedno dedikované vlákno, které bude komunikovat s\ ostatními procesy, přičemž stejná situace je\ u\ stávající komunikační vrstvy, která používá MPI.

Druhý možný scénář použití nového komunikačního rozhraní je, že\ každý proces bude spojen s\ ostatními procesy tolika datovými kanály, kolik pracovních vláken bude v\ rámci každého procesu spuštěno.^[To\ mimo jiné implikuje, že\ všechny procesy budou mít spuštěný stejný počet vláken.] Každé pracovní vlákno bude mít identifikační číslo v\ rámci procesu a\ sadu kanálů, které ho\ budou spojovat s\ vlákny stejného identifikačního čísla v\ jiných procesech. Vlákna tak spolu budou komunikovat jednak pomocí sdílené paměti v\ rámci jednoho procesu a\ každé vlákno může kontaktovat jedno vlákno z\ každého dalšího procesu, k\ němuž bude mít samostatný kanál.

Tyto způsoby použití nového komunikačního rozhraní se\ spolu s\ MPI podrobí experimentálnímu porovnání a\ na\ základě časového měření zhodnotím jejich chování. Pozornost budu zaměřovat kromě absolutních časových hodnot na\ schopnost škálovat, a\ to jak v\ počtu vláken na\ proces, tak v\ počtu procesů samých.

Měřeny budou celkem tři testy: test latence, test posílání krátkých zpráv a\ test posílání dlouhých zpráv. Všechny testy mají simulovat chování nástroje DIVINE, test latence spíše cílí na\ možný scénář komunikace v\ nové verzi nástroje DIVINE, zatímco zbylé dva simulují běh stávající verze DIVINE. Měření probíhala na\ strojích `pheme01` až\ `pheme16`.

## Test latence

Test latence probíhá tak, že\ každé vlákno postupně vygeneruje čísla od\ $1$ až\ po\ stanovené $N$, a\ posílá je\ na\ zpracování jinému procesu, které ho\ obratem vrátí zpátky, akorát vynásobené $-1$. Určení jiného procesu je\ dvojí -- buď je\ určeno na\ základě vygenerovaného čísla, nebo je\ určeno náhodně.

## Testy škálovatelnosti

Testy škálovatelnosti simulují průchod orientovaným grafem v\ obdobném duchu, jakým ho\ prochází nástroj DIVINE. Vrchol grafu je\ reprezentován dvěma čísly. V\ jednom z\ procesů dojde k\ vytvoření počátečního vrcholu grafu s\ nulovými hodnotami, který je\ poslán na\ zpracování. Zpracování probíhá tak, že\ je\ postupně jedno a\ druhé číslo z\ reprezentace vrcholu navýšeno o\ 1 a\ následně odesláno k\ dalšímu zpracování. Každý vrchol je\ na\ základě hashe jednoznačně přidělený procesu s\ určitým rankem.

V\ rámci testu je\ ukončení výpočtu zaručeno nejvyšší možnou hodnotou obou čísel reprezentující, po\ jejichž dosažení nedochází dále k\ předávání nově vytvořených vrcholů ke\ zpracování. V\ testu je\ použita hodnota $1000$, z\ čehož vyplývá, že simulace prohledá graf o\ $10^{6}$ vrcholech. Pro realističtější výsledky je\ ke\ každému vygenerování nového vrcholu připojen rekurzivní výpočet 25. fibonacciho čísla [[X]](https://oeis.org/A000045).

Oba testy probíhají stejně, jediné, v\ čem se\ liší, je\ velikost vrcholu v\ grafu. V\ případě krátké zprávy vrchol obsahuje pouze 3\ čísla, kdežto dlouhé zprávy mají velikost přes 1\ KB.

### Krátké zprávy

### Dlouhé zprávy



## Vyhodnocení

# Závěr

XXX
