Implementacja keyloggera jako sterownik w systemie Windows

Część pojęć może być błędnie przetłumaczona albo zostawiona bez tłumaczenia

**Jak działa odczytywanie klawiszy z klawiatury w systemie Windows?**

**Fizyczna budowa klawiatury**

Każdy klawisz ma przypisany do niego numer. Numer nie jest zależny od „wartości” klawisza. Oznacza to, że przy wciśnięciu klawisza jest wczytywany sam jego kod (scan code) i dopiero później zostanie on przetworzony na jakąś wartość. Wciśnięcie klawisza generuje zawsze dwa kody: kod wciśnięcia i zwolnienia klawisza. Dzięki temu, że mamy dwa kody, można sprawdzić czy jakiś klawisz jest wciśnięty przez dłuższy czas, co umożliwia wykrywanie jednoczesnego wciśnięcia kilku klawiszy, a także korzystania z funkcjonalności wymagających przetrzymania wciśniętego klawisza (np. bieganie w grach). 

**Co się dzieje po wciśnięciu klawisza na klawiaturze:**

1. Zamykany jest obwód elektryczny
1. Mikrokontroler klawiatury wykrywa, który klawisz został wciśnięty
1. Mikrokontroler klawiatury  generuje przerwanie sprzętowe i wysyła kod klawisza do komputera
1. Mikrokontroler na płycie głównej pobiera kod, konwertuje go i udostępnia jako output na porcie 60h. Następnie generuje przerwanie procesora.
1. Procedura przetwarzająca przerwanie klawiatury w systemie, pobiera kod z portu 60h.

**Jakie sterowniki odpowiadają za przetwarzanie danych wczytanych z klawiatury?**

W systemie Windows przetwarzanie danych z klawiatury odbywa się w trzech wastwach:

- Keyboard Class Driver (Kbdclass) – filtr klawiatury, sterownik klasowy
  - Jednoczesne wykonywanie operacji dla więcej niż 1 urządzenia
  - Obsługuje wszystkie urządzenia, niezależnie od szczegółów sprzętowych
- Keyboard Filter Driver – opcjonalny dodatkowy filtr klawiatury
  - Dane przetworzone przez filtr trafiają następnie do funkcjonalego sterownika
- I8042prt – funkcjonalny sterownik klawiatury
  - Zależny od sprzętu, obsługuje np. tylko klawiatury PS/2

**W jaki sposób przetwarzane są kody klawiszy?**

System Windows uzyskuje dostęp do klawiatury przy użyciu wątku Raw Input Thread (RIT), który jest częścią procesu csrrs.exe i jest tworzony podczas bootowania systemu. RIT umożliwia wysyłanie żądań do Keyboard Class Driver. 

Krok po kroku, jak jest przetwarzane wciśnięcie lub zwolnienie klawisza:

1. Kontroler klawiatury generuje przerwanie
1. Generowany jest sygnał IRQ1, który jest rejestrowany przez sterownik I8042prt
1. Procedura ISR odczytuje dane, które pojawiły się w wewnętrznej kolejce klawiatury
1. Wywoływana jest procedura KeyboardClassServiceCallback, która pobiera oczekujące żądania IRP z kolejki i umieszcza maksymalną ilość struktur KEYBOARD\_INPUT\_DATA, które zawierają informacje o wciśniętych klawiszach. 
1. RIT jest ponownie uruchamiany i wysyła kolejne żądania IRP do sterownika klasowego.

**W jaki sposób RIT przetwarza dane?**

1. Podczas uruchamiania systemu, Windows Explorer (explorer.exe) uruchamia wątek WinSta0\_RIT, który jest powiązany z RIT
1. Gdy użytkownik uruchomi pewien program, ten program zostanie automatycznie powiązany z RIT. W tym momencie proces Explorer odłącza od siebie RIT, ponieważ RIT może być powiązany tylko z jednym procesem jednocześnie. 
1. ` `Gdy użytkownik wciśnie klawisz na klawiaturze, pojawią się dane w kolejce SHIQ (System Hardware Input Queue), co prowadzi do uaktywnienia się RIT, który przekształca hardware input event w treść odczytaną z klawiatury, która zostanie przesłana do wątku aktywnego programu.
1. Program odczytuje dostarczone informacje z klawiatury przy użyciu funkcji GetMessage.

**Niektóre sposoby na napisanie keyloggera w trybie kernela**

**Użycie filtra klawiatury Kbdclass**

Można zainstalować filtr klawiatury, który będzie działał ponad sterownikiem klasowym klawiatury, czyli będzie pierwszym sterownikiem przetwarzającym klawisze. Klawisze będą przechwytywane bez względu na to, czy klawiatura jest podłączona na PS/2 czy przez USB. Taki keylogger jest najtrudniejszy do wykrycia, ponieważ można go zidentyfikować tylko przy użyciu innego sterownika. Wadą tego rozwiązania jest jego instalacja – trzeba zainstalować niepodpisany, niezidentyfikowany sterownik.

**Użycie filtra dla sterownika funkcjonalnego i8042prt**

Można również zainstalować keyloggera bezpośrednio ponad sterownikiem i8042prt. Polega to na zainstalowaniu sterownika ponad nienazwanym urządzeniem utworzonym przez sterownik i8042prt (to urządzenie nie musi fizycznie istnieć). Sterownik i8042prt jest interfejsem programowym do dodania dodatkowej funkcji przetwarzania przerwań IRQ1 (lsrRoutine). Przy użyciu tego interfejsu można przechwycić i analizować dane wczytane z klawiatury. Główną wadą tego rozwiązania jest to, że i8042prt jest zależny od urządzenia, przez co filtr będzie przechwytywał np. tylko input z urządzeń PS/2, ale z USB już nie.

**Modyfikacja kodu funkcji NtUserGetMessage lub NtUserPeekMessage** 

Taki keylogger przechwytuje żądania z klawiatury poprzez modyfikację kodu funkcji NtUserGetMessage lub NtUserPeekMessage. Te funkcje znajdują się w sterowniku systemowym win32k.sys i są wywoływane poprzez funkcje z biblioteki user32.dll. Z poziomu tych funkcji możliwe jest odczytanie wszystkich kodów klawiatury. Główną wadą tej metody jest bardzo trudna implementacja, dlatego takie keyloggery są ekstremalnie rzadkie.
