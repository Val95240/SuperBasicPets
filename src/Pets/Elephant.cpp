
#include "Pets/Elephant.hpp"

#include <string>

#include "utils.hpp"


Elephant::Elephant(Team* team, Shop* shop) : Pet("Elephant", team, shop) {
    repr = "🐘";
    id = 18;
    pack = PACK_STANDARD;
    tier = 2;
    attack = 3;
    life = 5;
    reset_stats();
}

Elephant::~Elephant() { }

void Elephant::on_before_attack() {
    int lvl = get_level();
    utils::vector_logs.push_back(name + " is gonna attack: before, hits " + std::to_string(lvl) + " pets behind");

    std::vector<Pet*> pets = team->get_pets();

    size_t index;
    for (size_t i=0; i<pets.size(); i++) {
        if (pets[i] == this) {
            index = i;
            break;
        }
    }
    index++;

    int nb_hurt = 0;
    for (size_t i=0; index+i<pets.size() && nb_hurt<lvl; i++) {
        if (pets[index+i]->is_alive()) {
            pets[index+i]->take_damage(1);
            nb_hurt++;
        }
    }
}
