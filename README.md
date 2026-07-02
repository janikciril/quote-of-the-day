Projektdokumentation: Quote of the Day

1. Projektübersicht & Idee

Das Projekt „Quote of the Day“ schlägt eine Brücke zwischen dem digitalen Raum und der physischen Welt. Über die Webanwendung hat die Polenta-Community die Möglichkeit, für ihr Lieblingszitat abzustimmen. Das Zitat, welches am Ende die meisten Stimmen erhält, verlässt den Browser und wird physisch im Raum auf einem LED-Matrix-Display präsentiert.

Das Ziel war es, Interaktion nicht nur auf einem Bildschirm stattfinden zu lassen, sondern ein greifbares, visuelles Erlebnis im echten Raum zu schaffen.

2. Systemarchitektur & Funktionsweise

Das Projekt besteht aus zwei Hauptkomponenten, die nahtlos miteinander kommunizieren müssen:

1. Das Frontend/Backend (Web): Eine Website, auf der die Zitate gesammelt und gevotet werden. Hier wird ermittelt, welches Zitat aktuell auf Platz 1 steht.
2. Die Hardware (Physical Computing): Ein ESP32-Mikrocontroller ruft die Daten (das Gewinner-Zitat) über eine API/Netzwerkverbindung ab und übersetzt diese in Steuersignale für die LED-Matrizen.

3. Verwendete Hardware

Für die physische Umsetzung kamen folgende Komponenten zum Einsatz:

* Mikrocontroller: ESP32 (verantwortlich für WLAN-Verbindung, Datenabruf und Steuerung der Displays)
* Displays: 2x LED-Matrizen (zusammengehängt für eine breitere Anzeigefläche)
* Verkabelung:Flachbandkabel zur Datenübertragung zwischen ESP32 und Matrizen
* Stromversorgung: Externes Netzteil (da die LEDs mehr Strom benötigen, als der ESP32 liefern kann)
* Platine: Lochrasterplatine für die feste Verlötung der Komponenten


4. Herausforderungen & Learnings

Die Umsetzung des Projekts brachte einige unerwartete Hürden mit sich, insbesondere im Bereich der Hardware, Elektronik und des Gehäusebaus.

Die Hardware-Integration & Verkabelung
Die erste technische Herausforderung bestand darin, das gesamte Setup – bestehend aus dem ESP32, den beiden kaskadierten LED-Matrizen und der externen Stromversorgung – stabil in Betrieb zu nehmen. Die korrekte Datenübertragung über die Flachbandkabel erforderte ein präzises Setup, da Signalstörungen bei LED-Matrizen sofort zu Bildfehlern oder Flackern führen.

Der Endgegner: Das Löten
Die mit Abstand größte Hürde war die physische Konstruktion auf der Lochrasterplatine. Da das Projektteam zuvor noch keinerlei Erfahrung im Löten hatte, war der Respekt vor dieser Aufgabe groß. Die vielen feinen Adern der Flachbandkabel mussten präzise auf der Lochrasterplatine mit den Pins des ESP32 verbunden werden.

Dies erforderte ein Maß an Feinmotorik und Fingerspitzengefühl, das wir uns erst durch Versuch und Irrtum mühsam aneignen mussten. Zu viel Lötzinn führte zu Kurzschlüssen zwischen den engen Pins, zu wenig Zinn zu Wackelkontakten (kalte Lötstellen). Dieser Prozess war extrem nervenaufreibend, lehrte uns aber enorm viel über Durchhaltevermögen und das systematische Debuggen von Hardware-Fehlern.

Wetterfestigkeit und Gehäusebau
Als ob die Elektronik nicht schon genug gewesen wäre, stellte uns das Gehäuse vor die nächste schwierige Aufgabe. Da die Box mit den LED-Matrizen im Freien oder in wechselnden Umgebungen eingesetzt werden soll, mussten wir das gesamte Konstrukt wetterfest machen. Das Abdichten der Gehäuseübergänge, der Schutz der empfindlichen LED-Fronten vor Feuchtigkeit und das sichere Herausführen der Kabelstränge ohne Wassereintritt erwies sich als handwerklich extrem anspruchsvoll und zeitintensiv.

5. Fazit

Trotz der anfänglichen Schwierigkeiten mit der Elektronik, dem Gehäusebau und den buchstäblichen „Schweißperlen“ beim Löten, war das Projekt ein Erfolg. Es ist absolut toll und befriedigend, das fertige Endprojekt nun in Aktion zu sehen – die Arbeit hat sich also sichtlich ausgezahlt.

Dennoch steht für uns als Team nach all den strapazierten Nerven, den Fehlversuchen und den verbrannten Fingerspitzen eines unumstößlich fest: Wir werden auf gar keinen Fall noch einmal ein ähnliches Hardware-Projekt auf die Beine stellen. Es war eine wertvolle, einmalige Erfahrung – und einmal reicht uns in diesem Fall völlig.
