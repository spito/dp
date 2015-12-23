# Úvod

## DIVINE

## "Cíl práce"


# Komunikační vrstva

Program DIVINE dokáže zpracovávat stavový prostor ve\ dvou režimech paralelizace. První z\ nich je paralelizace ve sdílené paměti, která je\ v\ aktuální verzi programu (DIVINE 3.x) upřednostňována. Některé důvody jako například možnost komprese stavového prostoru -- a\ tudíž efektivnější využívání paměti -- nebo rovnoměrnější rozvržení pracovní zátěže jednotlivých vláken -- což vede k\ rychlejšímu prohledávání stavového prostoru -- jsou popsány v\ [Vláďova bakalářka] a\ v\ [moje bakalářka].

Druhý režim paralelizace je\ hybridní a\ zahrnuje práci v\ distribuované i\ ve\ sdílené paměti. Tento režim pochází ze\ starší verze programu (DIVINE 2.x) a\ oproti původní verzi nebyl nikterak vylepšován (až\ na\ malou optimalizační změnu). Hybridní paralelizmus je\ realizován tak, že\ každý stav ze\ zpracovávaného stavového prostoru je\ staticky přiřazen některému vláknu na\ některé samostatné výpočetní jednotce pomocí hašování [odkaz na hash]; v\ nástroji DIVINE se používá konkrétně Spooky Hash[odkaz na Spookyhash]. Jako komunikační vrstva je\ použit standard MPI[odkaz na MPI], konkrétně implementace OpenMPI[odkaz na OpenMPI].

Hlavní nevýhodou původní implementace hybridního paralelizmu bylo statické rozdělení stavů nejen mezi jednotlivé výpočtní stroje ale\ i\ mezi jednotlivá vlákna. Toto rozdělení má\ kromě nevýhody v\ potenciálně nerovnoměrném rozložení práce mezi jednotlivá vlákna i\ nevýhodu v\ nemožnosti použít aktuální implementaci komprese paměti.

Již\ v\ průběhu vytváření režimu paralelizace ve\ sdílené paměti bylo zřejmé, že\ by\ bylo možné upravit stávající hybridní režim tak, aby\ v\ rámci jednotlivých výpočetních jednotek byl\ použit režim paralelizace ve\ sdílené paměti, kdežto pro\ rozdělení práce mezi výpočetní jednotky by\ nadále používalo statické rozdělování stavů na\ základě haše. Tento režim, pracovně nazvaný dvouvrstvá architektura, ovšem z\ důvodu upřednostnění jiných úkolů nebyl nikdy realizována.

V současné době se\ pracuje na\ nové verzi programu DIVINE, přičemž součástí změn je\ i\ úprava modelu paralelního zpracování stavového prostoru a\ zavedení jednotného režimu paralelizace pomocí dvouvrstvé architektury. Z\ tohoto důvodu bylo zvažováno, jestli by\ jiná komunikační vrstva nebyla jednodušší na\ použití a\ jestli by\ nebyla efektivnější při\ práci s\ pamětí. Další věc, kterou jsme zvažovali, byla co\ nejmenší závislost na\ externích knihovnách.

Před volbou vhodného komunikačního rozhraní bylo potřeba definovat, v\ jakém prostředí bude program DIVINE spouštěn, a\ tedy jaká jsou hlavní krutéria výběru. Očekáváme, že [`TBA`]

## MPI

### Vlastnosti

### Rozhraní v DIVINE

## Boost - asio

### Vlastnosti

## "Vlastní implementace"

Další možností je vlastní implementace komunikačního rozhraní pro DIVINE, které by\ používalo BSD sockety[odkaz na BSD sockety], které jsou v zahrnuty v [POSIX](http://pubs.opengroup.org/onlinepubs/9699919799/functions/contents.html) standardu. Vlastní implementace nepřidává žádnou závislost na externí knihovně a protože jsou BSD sockety v podstatě standardem pro síťovou komunikaci[dodat odkazy na pojednávající články], lze předpokládat, že výsledný kód bude možné bez větších změn použít i na operačních systémech, které nevycházejí z filozofie systému UNIX.

Popis BSD socketů je abstrakce nad různými druhy spojení. V\ části POSIX standardu o [socketech](http://pubs.opengroup.org/onlinepubs/9699919799/functions/V2_chap02.html#tag_15_10_06) můžeme nalézt poměrně nemálo věcí, které jsou specifikovány. Stěžejní z\ pohledu nástroje DIVINE a\ výběru vhodného komunikačního rozhraní jsou především dvě pasáže, a to `Address Famillies` a `Socket Types`. První definuje, skrz které médium se\ budou sockety používat, zatímco druhá podstatná pasáž je o tom, jaké vlastnosti bude mít samotný přenos dat skrz sockety.

Ačkoliv se\ v\ odkazované části POSIX standardu hovoří o\ "Address Families", dále ve\ standardu v\ části popisující funkce a\ jejich parametry se\ již hovoří o\ komunikační doméně. Budu tento termín nadále používat, neboť dle mého soudu lépe popisuje danou skutečnost.

### Komunikační doména

POSIX standard popisuje konkrétně tři možné komunikační domény -- lokální unixové sockety, sockety určené pro\ síťový protokol IPv4 [[RFC791]](https://tools.ietf.org/html/rfc791) a\ sockety pro síťový protokol IPv6 [[RFC2460]](https://tools.ietf.org/html/rfc2460). Standard se\ pak dále zabývá technickými detaily, jako které číselné konstanty jsou určeny pro kterou rodinu adres, či\ jak přesně jsou zápisy adres reprezentovány v\ paměti.

Pro\ adresaci unixových socketů se\ používá cesta adresářovým stromem, (Zde je nejspíše nutné malou poznámkou zmínit, že\ v\ souborových systémech unixového typu není adresářový strom stromem z\ pohledu teoretické informatiky, neboť umožňuje vytvářet cykly.) s\ tím, že\ standard nedefinuje maximální délku cesty. Žádné další aspekty adresování nejsou při práci unixovými sockety zohledňovány.

V\ případě socketů pro\ síťový protokol je\ třeba rozlišovat mezi protokolem IPv4 a\ IPv6. První zmiňovaný používá k\ adresaci čtyři čísla v\ rozsahu 0 -- 255. Při textové reprezentaci se\ ony\ čtyři čísla zapíšou v\ desítkovém základu a\ oddělí se\ tečkami. Ve\ strojovém zápisu se\ pak jedná o\ 32bitové číslo. Protokol IPv6 [[RFC4291]](https://tools.ietf.org/html/rfc4291) reflektuje primárně nedostatek adres IPv4 [[ICANN zpráva]](https://www.icann.org/en/system/files/press-materials/release-03feb11-en.pdf), což bylo při návrhu zohledněno a\ pro adresaci se\ používá osm čísel v rozsahu 0 -- 65 535. Preferovaná textová reprezentace je\ zapsat oněch osm čísel v\ šestnáctkovém základu a\ oddělit je\ dvojtečkami. Ve\ strojovém zápisu se\ pak jedná o 128bitvé číslo.

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

[^iana]: V roce 2002 bylo původní [[RFC1700]](https://tools.ietf.org/html/rfc1700) nahrazeno za [[RFC3232]](https://tools.ietf.org/html/rfc3232), které odkazuje na online databázi na adrese <http://www.iana.org>.

**[obrázek hlavičky paketu]**

##### Unixová doména

V\ unixové doméně lze\ spojité sockety chápat jako speciální soubory, které mohou být podobně jako soubory `FIFO` použity pro meziprocesovou komunikaci. Spojité sockety mají navíc od\ `FIFO` ty\ vlastnosti, že\ jsou obousměrné a\ že\ je\ možné je\ použít pro\ přenos zdrojů mezi procesy (například otevřené popisovače souborů).

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

Další zajímavou položkou z\ hlavičky UDP paketu je\ délka dat. Při znalosti této informace by\ bylo možné připravit přesně velkou paměť, do\ které by\ se data přijala. Rozhraní, které popisuje POSIX standard[^posix-recv] ale\ takovou možnost nenabízí, resp. tyto funkce vrací tuto hodnotu až\ poté, co došlo k\ přečtení zprávy. Je\ tedy nutné při navrhování protokolu, který bude používat UDP, zvážit nejvyšší možnou délku zprávy.

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

Touto cestou je\ možné implementovat libovolný vlastní protokol na\ čtvrté vrstvě ISO/OSI modelu. Moderní operační systém z\ důvodu bezpečnosti nedovolují aplikacím s\ běžným oprávněním vytváření takovýchto typů socketů, proto se\ jimi nebudu v\ této práci dále zaobírat.

[^raw-socket]: V originálu *raw sockets*.

# Nová implementace

## Analýza vlastností typů socketů

### Spojité sockety

Spojité sockety mají několik vlastností, ať\ už\ dobrých, či\ horších, které je\ třeba vzít v\ potaz:

*   udržování spojení
*   možnost mít více spojení mezi výpočetními stroji
*   garance doručení nepoškozených dat
*   režije

#### Udržování spojení

Distribuované výpočty, které nástroj DIVINE provádí, vyžadují, aby\ žádný z\ participatů výpočtu nepřerušil kontakt s\ ostatními. To\ se\ může stát jednak chybou v\ síti -- rozpojením sítě --, druhak zastavením výpočtu na\ stroji, ke\ kterému může dojít například z\ důvodu chyby v\ programu. U\ spojitých socketů jsou při\ přerušení spojení notifikování oba\ participanti komunikace, tudíž může dojít k\ následnému korektnímu ukončení distribuovaného neukončeného výpočtu.

Nevýhodou je\ nutnost pravidelné výměny zprávy, která říká, že\ spojení je\ stále aktivní, mezi participanty. Tato výměna je\ ale nezbytná pouze v\ případě, že\ v\ mezičase nebyla mezi participanty komunikace zaslána žádná zpráva. Vzhledem k\ předpokladu, že\ mezi výpočetními stroji bude docházet k\ čilé výměně dat, nepokládám tuto nevýhodu za\ kritickou.

#### Více spojení

Mezi dvěma komunikujícími stroji je\ v\ případě spojitých socketů navázat více než jedno spojení v\ rámci jednoho obslužného portu na\ straně serveru. Tento aspekt je\ možné vypozorovat z\ výše uvedené sekvence kroků, které vykonává server k\ přijímání a\ zpracování příchozách zpráv. Ustanovení více souběžných spojení může být\ potenciálně výhodné, neboť každé takové spojení můžeme chápat jako komunikační kanál a\ každý kanál použít pro\ jiný účel.

Lze tak například vytvořit jeden kanál pro\ posílání řídících zpráv, další kanály pak\ pro\ posílání dat. Výhodou navíc může být, že\ každé vlákno nástroje DIVINE bude obsluhovat svůj vlastní datový kanál bez nutnosti explicitního zamykání přístupu ke komunikačnímu zdroji.

Vzhledem k\ tomu, jak nástroj DIVINE pracuje v\ distribuovaném výpočtu s\ daty, které posílá na\ zpracování a\ které naopak přijímá,a\ vzhledem k\ očekávaným velikostem datových balíčků, je\ výhodné se\ vyhnout častému kopírování dat. Přístup více spojení přenechává operačnímu systému rozhodování o\ přidelěných vyrovnávacích pamětech, což\ se\ může projevit v\ efektivnější práci s\ pamětí, protože bude docházet k\ menšímu počtu nutného kopírování bloků dat.

#### Garance doručení nepoškozených dat

### Nespojité sockety

#### Vlastnost 1

#### Vlastnost 2

### Sekvenční sockety

#### Vlastnost 1

#### Vlastnost 2

## Bezpečnost

## Protokol

### Ustanovení sítě

### Rozpuštění sítě

### Řešení chyb a problémových stavů



## Architektura



## Rozhraní pro distribuované procházení grafu

# Experimentální porovnání

## Test latence

## Test škálovatelnosti -- krátké zprávy

## Test škálovatelnosti -- dlouhé zprávy

# Závěr

