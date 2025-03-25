# scheduler
Celem projektu jest napisanie programu do harmonogramowania zadań będącego funkcjonalnym odpowiednikiem programu cron (https://pl.wikipedia.org/wiki/Cron) znanego z systemów unixowych. Program powinien spełniać następujące założenia:

- powinien być oparty na mechanizmie zegarów interwałowych;
- sterowanie pracą programu powinno odbywać przez argumenty wiersza poleceń;
- powinno być możliwe uruchomienie tylko jednej instancji programu (pierwsze uruchomienie pliku wykonywalnego uruchamia serwer usługi, kolejne są klientami);
- kolejne uruchomienia pliku wykonywalnego z odpowiednimi argumentami służyć mają sterowaniu pracą programu;
- komunikacja klient-serwer ma przebiegać z wykorzystaniem mechanizmu komunikatów;
- powinno być możliwe planowanie uruchomienia wskazanego programu poprzez podanie czasu względnego, bezwzględnego lub cyklicznie;
- musi istnieć możliwość planowanego uruchomienia programu z argumentami wiersza poleceń;
- program musi mieć możliwość wyświetlenia listy zaplanowanych zadań;
- musi istnieć możliwość anulowania wybranego zadania;
- program powinien mieć możliwość zapisywania logów z wykorzystaniem biblioteki zrealizowane w ramach projektu 1;
- zakończenie pracy programu powinno być równoznaczne z anulowaniem wszystkich zaplanowanych zadań;
- aplikacja powinna być zgodna ze standardami POSIX i języka C.
