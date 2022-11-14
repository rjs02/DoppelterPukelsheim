# Doppelter Pukelsheim

Das Programm berechnet die doppeltproportionale Sitzverteilung nach Pukelsheim.

# Input
`.csv`-File im folgenden Format:

| Titel | $q$ | Parteien |
| :---: | :---: | :---: |
| Wahlkreise | Sitze | Parteistimmen |

$q \in \set{0, 1, 2, 3}$ spezifiert das Mindestquorum, das angewendet werden soll:

<!-- create table with 2 cols 5 rows -->
| $q$ | Quorum |
| :---: | :---: |
| 0 | kein Mindestquorum |
| 1 | Mind. 5% der Stimmen in mind. 1 Wahlkreis |
| 2 | Mind. 3% der Stimmen insgesamt |
| 3 | Mind 5% in 1 WK *oder* 3% insg. |

# Literatur
- Das neue Zürcher Zuteilungsverfahren für Parlamentswahlen: https://www.math.uni-augsburg.de/htdocs/emeriti/pukelsheim/2004b.pdf
- Der doppelte Pukelsheim: https://www.youtube.com/watch?v=hXjSmwTFJpc
