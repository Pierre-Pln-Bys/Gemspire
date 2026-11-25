# Passe de saison — cadrage (brouillon à valider)

Document de préparation, rien n'est codé. Les choix marqués **[à décider]**
attendent la décision du propriétaire du projet.

## Principes (validés)

- Un passe par **saison** (deux mois, mêmes dates que les saisons classées).
- **50 paliers**, une **piste gratuite** pour tous et une **piste premium**
  payante.
- **Cosmétique seulement** : aucune récompense ne donne d'avantage en partie
  (règles, IA, Elo et recherche de partie inchangés).
- XP gagnée par les **parties en ligne**, les **défis de la semaine** et un
  **bonus de connexion quotidien**.
- Récompenses : **thèmes du plateau**, **dos de cartes et jetons**, **cadres de
  profil et titres**, **avatars et réactions du chat**.

## Gain d'XP (proposition, à équilibrer après essai)

| Source | XP | Limites (contre l'abus) |
|---|---|---|
| Partie classée ou de saison terminée | 100, +50 si victoire | — |
| Partie amicale terminée | 50 | 5 par jour (sinon : deux amis qui enchaînent) |
| Abandon / 3 tours manqués | 0 | — |
| Première partie en ligne du jour | +150 | 1 par jour |
| Défi de la semaine | 400 à 600 | 3 défis par semaine, renouvelés le lundi |
| Parties contre l'ordinateur | 0 | non vérifiables par le serveur |

Un palier = **700 XP** (35 000 XP pour les 50). Sur une saison de ~8,5
semaines, un joueur qui fait ses défis, joue un jour sur deux et ~6 parties
par semaine atteint environ le palier 40 ; les joueurs très réguliers finissent
le passe. Les valeurs restent des constantes faciles à régler.

Exemples de défis (tirés au hasard chaque semaine, la même liste pour tous) :
gagner 2 parties de saison, attirer 3 nobles, acheter 5 cartes bleues, finir
une partie à 18 points ou plus, gagner sans réserver, jouer avec un ami...
Ils réutilisent ce que calculent déjà les succès (`achievements.cpp`).

## Récompenses (proposition)

- **Piste gratuite** : un objet tous les 5 paliers (10 objets) — par exemple
  2 thèmes, 2 titres, 2 avatars, 1 dos de cartes, 1 cadre, 2 réactions.
- **Piste premium** : un objet à chaque palier (50 objets), dont un ensemble
  complet de saison (thème + dos de cartes + jetons + cadre + titre) au palier 50.
- Les objets gagnés **restent** après la saison ; les objets d'une saison ne
  reviennent pas plus tard (ou seulement en version recolorée) **[à décider]**.
- Les titres et cadres sont visibles dans les salons, le classement, les
  parties regardées et sur Discord (`/profile`).

Quantité de dessins par saison : ~60 objets. Le jeu dessine déjà tout en
code (raylib, pas d'images) : thèmes, dos de cartes et jetons peuvent rester
générés par code (couleurs, motifs) ; avatars et cadres demanderont soit des
images, soit des motifs dessinés **[à décider]**.

## Piste premium : ce qu'il faut avant de vendre

Vendre, même 3 €, change la nature du projet. À vérifier **avant** de coder la
partie paiement :

1. **Statut pour vendre** : il faut une structure qui encaisse (par exemple une
   micro-entreprise avec SIRET). Vérifier aussi le règlement de l'école sur un
   projet étudiant commercialisé (propriété du code, encadrement).
2. **Prestataire de paiement** : le plus simple est un **« Merchant of Record »**
   (Paddle, Lemon Squeezy...) qui vend en son nom, gère la TVA de chaque pays
   de l'UE, les factures et les remboursements. Stripe seul est moins cher mais
   laisse la TVA (guichet OSS), les factures et les litiges à votre charge.
3. **Documents** : conditions générales de vente, droit de rétractation (contenu
   numérique fourni tout de suite : renonciation expresse du joueur au moment
   de l'achat), politique de remboursement, mise à jour de la politique de
   confidentialité (données de paiement chez le prestataire).
4. **Mineurs** : âge minimum pour acheter ou accord parental ; pas de pression
   à l'achat (pas de compte à rebours agressif, prix clair).
5. **Plateformes** : les achats faits **dans** une version Steam doivent passer
   par Steam ; l'Activité Discord a ses propres règles de monétisation (à
   vérifier avant d'y proposer l'achat). Le plus sûr au début : achat
   uniquement sur le site (navigateur), le jeu ne fait qu'afficher le statut.
6. **Prix** **[à décider]** : 3 à 5 € par saison.

Ces points dépendent de votre situation : à faire valider par quelqu'un de
compétent (école, expert-comptable) avant la mise en vente.

## Côté technique (aperçu)

- **Serveur** (SQLite) : `pass_progress` (compte, saison, XP, premium),
  `cosmetics` (objets possédés), `equipped` (objets portés), `weekly_challenges`
  (défis de la semaine, tirés à partir du numéro de semaine), progression des
  défis par compte. L'XP est calculée **par le serveur** à la fin d'une partie
  (`Rooms::finish_game`), jamais envoyée par le jeu.
- **Catalogue des objets et paliers** : données dans le code (comme les cartes
  et les succès), une liste par saison.
- **Paiement** : page d'achat sur le site → prestataire → **webhook signé**
  vers le serveur qui active le premium. Le jeu n'envoie jamais « j'ai payé ».
- **Jeu** : écran « Passe de saison » (50 paliers, piste gratuite et premium,
  défis de la semaine), écran « Apparence » (choisir thème, dos de cartes,
  jetons, cadre, titre, avatar), barre d'XP en fin de partie.
- **Bot** : `/profile` avec le titre et le palier ; annonce des défis de la semaine.

## Étapes proposées

1. **Piste gratuite** : XP des parties et bonus quotidien, 50 paliers, premiers
   objets simples (thèmes, titres), écrans Passe et Apparence.
2. **Défis de la semaine**.
3. **Dos de cartes, jetons, cadres, avatars, réactions**.
4. **Piste premium** : seulement quand les points de la section précédente sont
   réglés (statut, prestataire, documents).

## Questions ouvertes

- Les objets d'une saison passée peuvent-ils revenir ?
- Avatars et cadres : images dessinées (par qui ?) ou motifs générés par code ?
- Prix du premium, et achat possible en cours de saison (avec rattrapage des
  paliers déjà atteints : oui, logique) ?
- Peut-on acheter des paliers (« sauter des paliers ») ? Proposition : non.
