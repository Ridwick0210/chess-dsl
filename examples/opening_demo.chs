# The symbol table resolves KingsPawn when the game uses it.
opening KingsPawn {
    move e2 -> e4;
    move e7 -> e5;
}

game OpeningDemo {
    use KingsPawn;
    move g1 -> f3;
    move b8 -> c6;
}
