# Inspect use-site expansion and a non-fatal unused-opening warning.
opening KingsPawn {
    move e2 -> e4;
    move e7 -> e5;
}

opening UnusedLine {
    move d2 -> d4;
    move d7 -> d5;
}

game InspectionDemo {
    use KingsPawn;
    move g1 -> f3;
    move b8 -> c6;
}
