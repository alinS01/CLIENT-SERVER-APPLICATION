Alin Stan
322 CB

Aceasta este o aplicatie server-client care utilizeaza socket-uri TCP si UDP pentru transmiterea mesajelor. 
Subscriberii pot sa se aboneze la diferite subiecte si pot primi mesaje de la acestea.

Componentele aplicatiei includ serverul, abonatul si un stream UDP.
Serverul realizeaza conexiunea intre clientii TCP si UDP si initializeaza socket-urile TCP si UDP si adresa serverului.

Abonatul este un client TCP care se conecteaza la server si initializeaza socket-ul pasiv TCP si adresa serverului. 
Pentru fiecare iteratie, selecteaza socket-ul potrivit pentru a asigura functionalitatea corecta a clientului si 
poate trimite comenzi de subscriere, dezabonare sau poate inchide clientul.

Am utilizat mai multe structuri pentru a facilita transportul. Pentru a vorbi de la clientul udp cu serverul,
am o strctura udp_msg care incasuleaza informatia care urmeaza sa fie trimisa catre clientul abonat.

Serverul tine informatia in array-uri de clienti si topicuri. Clientul este definit printr-un id unic,
file descriptorul pe care comunica. In el am un camp pentru a sti daca este online sau nu.

Topicul contine un vector de abonati care sunt abonati la topicul respectiv si numele sau.

Atunci cand un client TCP incearca sa se conectez, iteram prin vectorul de abonati pentru a vedea daca il gasim dupa
ID acolo, in cazul in care il gasim si este deja online, nu il lasam sa se conecteze. Daca il gasim si este
marcat ca offline, il reconectam. Altfel, creeam un nou abonat pe care il salvam.

Un topic nou se creeaza de fiecare data cand un abonat da subscribe, atata timp cat nu exista deja.
Dupa creearea topicului, adaug subscriberul care a cerut abonarea la lista de abonati ai topicului.

Cand primesc un topic de la un client UDP, iterez prin lista de topicuri pentru a-l gasi pe cel care are acelasi nuem,
dupa care il trimit la toti abonatii din lista de abonati.
