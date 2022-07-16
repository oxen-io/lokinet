Shim Library for Streamlined NTRU Prime 4591^761

original code was written by:

* Daniel J. Bernstein
* Chitchanok Chuengsatiansup
* Tanja Lange
* Christine van Vredendaal

we have hacked this code up too much and will eventually be replacing it with another pq keyexchange algorithm.
when we do our next major protocol flag day for clients, we hope to check what the best pq keyexchange is and go with it.
that being said, 
we are using this onesince its role is providing resistence towards passive data collection and decryption in a future where ecdlp is broken and is doing fine for now.
if there is ever a time when ecdlp is breakable in real time, 
we will need to shift off ed25519 being used as .loki addresses to something that can handle a break in ecdlp. 
the .loki address pubkey signs the ciphertext containing the sntrup pubkey, which would break it entirely. 
however, if collected now and decrypted in the future, efforts likely will not advance beyond there as this component key exchange is assumed to be (probably) non reversable from a break in ecdlp.
(unless it's not and it is broken via other means, which has occurred with lattice based primatives before)

a tl;dr for the armchair cryptographers:

this code is a best effort temp stopgap placeholder providing SOME pq resistence that exists until pq cryptographic primatives mature.
