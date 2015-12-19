# Úvod

## Explicitní model-checking

## DIVINE



# Komunikační rozhraní

Program DIVINE dokáže zpracovávat stavový prostor ve\ dvou režimech paralelizace. První z\ nich je paralelizace ve sdílené paměti, která je\ v\ aktuální verzi programu (DIVINE 3.x) upřednostňována. Některé důvody jako například možnost komprese stavového prostoru -- a\ tudíž efektivnější využívání paměti -- nebo rovnoměrnější rozvržení pracovní zátěže jednotlivých vláken -- což vede k\ rychlejšímu prohledávání stavového prostoru -- jsou popsány v\ [Vláďova bakalářka] a\ v\ [moje bakalářka].

Druhý režim paralelizace je hybridní a zahrnuje práci v distribuované i ve sdílené paměti. Tento režim pochází ze\ starší verze programu (DIVINE 2.x) a oproti původní verzi nebyl nikterak vylepšován (až na malou optimalizační změnu). Hybridní paralelizmus je realizován tak, že každý stav ze zpracovávaného stavového prostoru je staticky přiřazen některému vláknu na některé samostatné výpočetní jednotce pomocí hašování [odkaz na hash]; v DIVINE se používá konkrétně Spooky Hash[odkaz na Spookyhash]. Jako komunikační vrstva je použit standard MPI[odkaz na MPI], konkrétně implementace OpenMPI[odkaz na OpenMPI].

Hlavní nevýhodou původní implementace hybridního paralelizmu bylo statické rozdělení stavů nejen mezi jednotlivé výpočtní stroje ale i mezi jednotlivá vlákna. Toto rozdělení má kromě nevýhody v potenciálně nerovnoměrném rozložení práce mezi jednotlivá vlákna i nevýhodu v nemožnosti použít aktuální implementaci komprese paměti[Vláďova bakalářka].

Již v průběhu vytváření režimu paralelizace ve sdílené paměti bylo zřejmé, že by bylo možné upravit stávající hybridní režim tak, aby v rámci jednotlivých výpočetních jednotek byl použit režim paralelizace ve sdílené paměti, kdežto pro rozdělení práce mezi výpočetní jednotky by nadále používalo statické rozdělování stavů na základě haše. Tento režim, pracovně nazvaný dvouvrstvá architektura, ovšem z důvodu upřednostnění jiných úkolů nebyl nikdy realizována.

V současné době se pracuje na nové verzi programu DIVINE, přičemž součástí změn je i úprava modelu paralelního zpracování stavového prostoru a zavedení jednotného režimu paralelizace pomocí dvouvrstvé architektury. Z tohoto důvodu bylo zvažováno, jestli by jiná komunikační vrstva nebyla jednodušší na použití a jestli by nebyla efektivnější při práci s pamětí. Další věc, kterou jsme zvažovali, byla co nejmenší závislost na externích knihovnách.

Před volbou vhodného komunikačního rozhraní bylo potřeba definovat, v jakém prostředí bude program DIVINE spouštěn, a tedy jaká jsou hlavní krutéria výběru. Očekáváme, že [`TBA`]

## MPI

### Vlastnosti

### Rozhraní v DIVINE

## Boost - asio

### Vlastnosti

## Vlastní implementace

Další možností je vlastní implementace komunikačního rozhraní pro DIVINE, které by\ používalo BSD sockety[odkaz na BSD sockety], které jsou v zahrnuty v [POSIX](http://pubs.opengroup.org/onlinepubs/9699919799/functions/contents.html) standardu. Vlastní implementace nepřidává žádnou závislost na externí knihovně a protože jsou BSD sockety v podstatě standardem pro síťovou komunikaci[dodat odkazy na pojednávající články], lze předpokládat, že výsledný kód bude možné bez větších změn použít i na operačních systémech, které nevycházejí z filozofie systému UNIX.

### BSD sockety

Popis BSD socketů je abstrakce nad různými druhy spojení. V\ části POSIX standardu o [socketech](http://pubs.opengroup.org/onlinepubs/9699919799/functions/V2_chap02.html#tag_15_10_06) můžeme nalézt poměrně nemálo věcí, které jsou specifikovány. Stěžejní z\ pohledu nástroje DIVINE a\ výběru vhodného komunikačního rozhraní jsou především dvě pasáže, a to `Address Famillies` a `Socket Types`. První definuje, skrz které médium se\ budou sockety používat, zatímco druhá podstatná pasáž je o tom, jaké vlastnosti bude mít samotný přenos dat skrz sockety.

Ačkoliv se\ v\ odkazované části POSIX standardu hovoří o\ "Address Families", dále ve\ standardu, v\ části popisující funkce a\ jejich parametry, se\ již hovoří o\ komunikační doméně. Budu tento termín nadále používat, neboť dle mého soudu lépe popisuje danou skutečnost.

#### Komunikační doména

POSIX standard popisuje konkrétně tři možné komunikační domény -- lokální UNIXové sockety, sockety určené pro\ síťový protokol IPv4 [[RFC791]](https://tools.ietf.org/html/rfc791) a\ sockety pro síťový protokol IPv6 [[RFC2460]](https://tools.ietf.org/html/rfc2460). Standard se\ pak dále zabývá technickými detaily, jako které číselné konstanty jsou určeny pro kterou rodinu adres, či\ jak přesně jsou zápisy adres reprezentovány v\ paměti.

Pro\ adresaci UNIXových socketů se\ používá cesta adresářovým stromem, (Zde je nejspíše nutné malou poznámkou zmínit, že\ v\ souborových systémech UNIXového typu není adresářový strom stromem z\ pohledu teoretické informatiky, neboť umožňuje vytvářet cykly.) s\ tím, že\ standard nedefinuje maximální délku cesty. Žádné další aspekty adresování nejsou při práci UNIXovými sockety zohledňovány.

V\ případě socketů pro\ síťový protokol je\ třeba rozlišovat mezi protokolem IPv4 a\ IPv6. První zmiňovaný používá k\ adresaci čtyři čísla v\ rozsahu 0 -- 255. Při textové reprezentaci se\ ony\ čtyři čísla zapíšou v\ desítkovém základu a\ oddělí se\ tečkami. Ve\ strojovém zápisu se\ pak jedná o\ 32bitové číslo. Protokol IPv6 [[RFC4291]](https://tools.ietf.org/html/rfc4291) reflektuje primárně nedostatek adres IPv4 [[ICANN zpráva]](https://www.icann.org/en/system/files/press-materials/release-03feb11-en.pdf), což bylo při návrhu zohledněno a\ pro adresaci se\ používá osm čísel v rozsahu 0 -- 65 535. Preferovaná textová reprezentace je\ zapsat oněch osm čísel v\ šestnáctkovém základu a\ oddělit je\ dvojtečkami. Ve\ strojovém zápisu se\ pak jedná o 128bitvé číslo.

Vzhledem k\ tomu, že\ v\ současné době oba protokoly koexistují vedle sebe a\ nelze bez podrobnějšího zkoumání rozhodnout, zda nějaká síť používá IPv4, či\ IPv6, je\ nutné v\ POSIXovém rozhraní nabídnout takovou funkcionalitu, která bude (pro běžné používáním) nezávislá na\ verzi protokolu IP. Touto funkcí je\ [`getaddrinfo`](http://pubs.opengroup.org/onlinepubs/9699919799/functions/getaddrinfo.html), která přeloží jméno cílového stroje a/nebo jméno služby a\ vrací seznam adres, lhostejno zda IPv4, či\ IPv6, které lze dále použít ve\ funkcích řešících otevření spojení, zaslání dat a\ jiných.

Jméno cílového stroje obyčejně bývá název stroje v\ síti, což může také zahrnout i\ nadřazené domény. Více informací lze hledat v\ [[RFC1034]](https://tools.ietf.org/html/rfc1034), [[RFC1035]](https://tools.ietf.org/html/rfc1035) (definují koncept doménových jmen a popisují jejich implementaci) a\ [[RFC1886]](https://tools.ietf.org/html/rfc1886) (rozšíření doménových jmen pro IPv6). Co je\ jméno služby proberu více u\ typů socketů.


#### Typy socketů

POSIX standard definuje čtyři různé typy: `SOCK_DGRAM`, `SOCK_RAW`, `SOCK_SEQPACKET` a\ `SOCK_STREAM` s\ tím, že\ implementace může definovat další typy. Ne\ všechny typy socketů jsou k\ dispozici ve všech komunikačních doménách, navíc je\ nutné mít na paměti, že\ výsledné chování každého typu může navíc záležet na\ komuniační doméně. Proto je\ potřeba na základě jejich vlastností vybrat takový typ, které by\ nejlépe vyhovoval požadavkům nástroje DIVINE.

Stručné představení jednotlivých typů:

*   Sockety typu `SOCK_STREAM` -- spojité sockety -- vytváří spojení, dávají garance doručení a\ správnosti dat. Datový přenos se\ chápe jako proud.
*   Sockety typu `SOCK_SEQPACKET` -- sekvenční sockety -- jsou téměř shodné se\ spojitými sockety, s\ tím rozdílem, že\ datový přenos chápou jako jednotlivé zprávy, nikoliv jako proud.
*   Sockety typu `SOCK_DGRAM` -- nespojité sockety -- nevytváří spojení, nedávají žádné garance a\ datový přenos chápou jako jednotlivé zprávy.
*   Sockety typu `SOCK_RAW` je\ velmi podobný nespojitým socketům s\ tím rozdílem, že\ nad\ tímto typem jsou postaveny všechny další typy socketů.

##### Spojité




Spojité sockety jsou známé svým použitím v\ implementaci protokolu TCP[odkaz]. Komunikace tímto způsobem probíhá tak, že výpočetní stroj, který očekává příchozí spojení (dále jako server), otevře na své straně na určeném portu socket[[bind]](http://linux.die.net/man/2/bind)[[listen]](http://linux.die.net/man/2/listen), skrze něhož hodlá zpracovávat příchozí spojení. Výpočetní stroj, který chce se serverem komunikovat (dále jako klient), požádá[connect] o spojení na server na předem definovaný port, následkem čehož je server notifikován a může přijmout[accept] příchozí spojení. Přijetím spojení se vytvoří na straně serveru další socket, který pak slouží jako jeden konec obousměrné komunikace, a obdobně získá klient na své straně socket pro komunikaci se serverem. Pomocí získaného socketu lze obousměrně komunikovat jak standardními POSIXovými funkcemi[read,write] tak funkcemi specifickými pro práci se sockety[send,recv], případně měnit vlastnosti socketu[setsockopt].

Pozorného čtenáře by mohlo napadnout, jak je v operačním systému adresováno právě získané spojení. Adresace je vyřešena tak, že jak na straně klienta tak na straně serveru se vytvoří nové sockety, přičemž na straně klienta je socket otevřen na nějakém volném portu, který vybírá operační systém. Není proto na klientovi nutné specifikovat konkrétní port pro dané spojení.

Data se mezi dvěma konci spojení posílají pomocí paketů[packet]. Spojité sockety garantují{footnote: Je garance opravdu možná?}, že spojení bude udržováno, dokud ho jedna z komunikujících stran neuzavře. Dále garantují, že odeslaná data dojdou a že dojdou ve správném pořadí, tj. v takovém, v jakém byly odeslány. Pro implementace všech těchto garancí se využívá hlavička paketů[hlavička paketu], ve které se definují věci jako pořadí zprávy, kontrolní součet dat posílaných ve zprávě a mnohé další. Pro udržování spojení se pak používá speciální paket, který si musí komunikující strany v pravidlených intervalech posílat, ovšem je možné tento speciální paket vložit do běžné datové zprávy.

[obrázek hlavičky paketu]

Níže je sepsán seznam vlastností spojitého komunikačního kanálu, které jsou z hlediska použití programem DIVINE klíčové:

*   udržování spojení
*   možnost mít více kanálů mezi výpočetními stroji
*   garance doručení nepoškozených dat

###### Udržování spojení

Distribuované výpočty, které program DIVINE provádí, vyžadují, aby žádný z participatů výpočtu nepřerušil kontakt s ostatními. To se může stát jednak chybou v síti -- rozpojením sítě --, druhak zastavením výpočtu na stroji, ke kterému může dojít z důvodu chyby v programu, případně z důvodu

###### Více kanálů


###### Garance doručení nepoškozených dat



##### Nespojité



##### ???(třetí způsob)


### Vlastní definice workflow

### XXX

### Popis komunikačního protokolu


### Bezpečnost

# Rozhraní pro distribuované procházení grafu



# Experimentární porovnání

# Závěr
