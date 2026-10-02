# Algoritmeanalyse og Optimering
## HTX SOP Rapport - Matematik A & Programmering B

## Indholdsfortegnelse
1. [Abstract](#abstract)
2. [Problemformulering](#problemformulering)
3. [Teori](#teori)
4. [Metode](#metode)
5. [Implementering](#implementering)
6. [Analyse](#analyse)
7. [Diskussion](#diskussion)
8. [Konklusion](#konklusion)
9. [Perspektivering](#perspektivering)
10. [Litteraturliste](#litteraturliste)

## Abstract
Denne SOP undersøger algoritmers tidskompleksitet gennem matematisk analyse og praktisk implementering. Projektet fokuserer på sorteringsalgoritmerne Merge Sort og Quick Sort, samt søgealgoritmen Binary Search. Gennem matematisk modellering og empirisk dataindsamling undersøges sammenhængen mellem teoretisk kompleksitet og praktisk ydeevne. Resultaterne visualiseres og analyseres for at forstå algoritmernes effektivitet under forskellige forhold.

## Problemformulering

Hvordan kan man gennem matematisk analyse og praktisk implementering undersøge og visualisere forskellige algoritmers tidskompleksitet og ydeevne? Specifikt ønskes det at:

1. Analysere og bevise tidskompleksiteten for Merge Sort, Quick Sort og Binary Search gennem matematisk modellering
2. Implementere algoritmerne og udvikle et benchmarking-system til empirisk analyse
3. Visualisere og sammenligne teoretisk og praktisk ydeevne
4. Undersøge hvordan input-størrelse og -type påvirker algoritmernes køretid

## Teori

### Matematisk Grundlag

Matematisk udtrykt:
```
f(n) = O(g(n)) hvis ∃ c, n₀ > 0: 0 ≤ f(n) ≤ c·g(n) for alle n ≥ n₀
```

### Sorteringsalgoritmer

#### Merge Sort
Merge Sort er baseret på "del og hersk" princippet med følgende karakteristika:
- Tidskompleksitet: O(n log n) i alle tilfælde
- Pladskompleksitet: O(n)
- Stabil sortering

Matematisk rekursionsformel:
```
T(n) = 2T(n/2) + O(n)
```

#### Quick Sort
Quick Sort bruger også "del og hersk" men med en anden strategi:
- Bedste/gennemsnitlig tidskompleksitet: O(n log n)
- Værste tidskompleksitet: O(n²)
- Pladskompleksitet: O(log n)

## Metode

### Implementering
Projektet er implementeret i C++ med følgende hovedkomponenter:

1. Instrumentering af kode med tidsmåling
2. Implementering af sorteringsalgoritmer
3. Visualisering af resultater med Raylib

### Benchmarking Metodik
For hver algoritme testes:
- Forskellige inputstørrelser (n)
- Forskellige input-scenarier (bedste, gennemsnitlige, værste tilfælde)
- Multiple kørsler for at reducere tilfældige variationer

## Analyse

### Teoretisk Analyse

#### Merge Sort Kompleksitet
Matematisk bevis for O(n log n):
1. Rekursiv opdeling: log₂(n) niveauer
2. Merge operation på hvert niveau: O(n)
3. Total: O(n) × O(log n) = O(n log n)

#### Quick Sort Kompleksitet
Matematisk analyse af værste tilfælde:
1. Pivot vælges altid som mindste/største element
2. Rekursionsdybde: n
3. Partition på hvert niveau: O(n)
4. Total: O(n) × O(n) = O(n²)

### Praktisk Analyse

Baseret på vores benchmarking resultater observerer vi:

1. Merge Sort viser konsistent ydeevne:
   - Næsten identisk køretid for alle input-typer
   - Følger tæt den teoretiske O(n log n) kurve

2. Quick Sort viser varierende ydeevne:
   - Bedste/gennemsnitlige tilfælde: Hurtigere end Merge Sort
   - Værste tilfælde: Markant dårligere ydeevne

[Indsæt graf fra din visualisering her]

## Konklusion

Gennem dette projekt har vi:
1. Bekræftet den teoretiske kompleksitet gennem praktiske målinger
2. Observeret hvordan forskellige input påvirker algoritmernes ydeevne
3. Vist værdien af både matematisk analyse og praktisk benchmarking

Resultaterne viser at:
- Merge Sort er mere pålidelig med garanteret O(n log n)
- Quick Sort er generelt hurtigere men mere sårbar overfor særlige input

## Litteraturliste

1. Cormen, T. H., et al. (2009). Introduction to Algorithms (3rd ed.). MIT Press.
2. Sedgewick, R., & Wayne, K. (2011). Algorithms (4th ed.). Addison-Wesley.
3. [Andre relevante kilder...]
