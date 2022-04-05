
#include "team.hpp"

#include <fstream>
#include <iostream>

#include "utils.hpp"


TeamList Team::team_list;

Team* Team::unserialize(std::string team_str) {
    Team* new_team = new Team();
    int index = team_str.find(' ');
    int turn = std::stoi(team_str.substr(0, index));
    team_str = team_str.substr(index);

    while (!team_str.empty()) {
        index = team_str.find(')');
        std::string pet_str = team_str.substr(2, index-2);
        team_str = team_str.substr(index+1);

        new_team->_add(Pet::unserialize(new_team, pet_str));
    }
    new_team->tmp_pets = new_team->pets;
    new_team->turn = turn;
    new_team->in_fight = true;

    return new_team;
}

Team* Team::get_random_team(int turn) {
    if (Team::team_list.empty())
        Team::load_teams();

    int opp_turn = turn;
    while (team_list[opp_turn].empty())
        opp_turn--;

    Team* opp_team = utils::choice(team_list[opp_turn])[0];
    return opp_team;
}

void Team::clear_team_list() {
    for (auto [turn, teams] : Team::team_list) {
        for (Team* team : teams)
            delete team;
    }
    Team::team_list.clear();
}


Team::Team() : turn(0) { }

Team::~Team() {
    for (Pet* pet : pets)
        delete pet;
}

size_t Team::get_nb_pets() const {
    return pets.size();
}

void Team::can_combine(size_t index, std::string other_pet) const {
    check_size("COMBINE", index);
    if (pets[index]->name != other_pet)
        throw InvalidAction("[COMBINE]: trying to combine different pets");
}

void Team::can_combine(size_t src_index, size_t dst_index) const {
    check_size("COMBINE", src_index);
    if (src_index == dst_index)
        throw InvalidAction("[COMBINE]: same source (" + std::to_string(src_index+1) + ")" + \
                            " and destination (" + std::to_string(dst_index+1) + ")");
    can_combine(dst_index, pets[src_index]->name);
}

bool Team::is_fighting() const {
    return in_fight;
}

void Team::begin_turn() {
    turn++;
    for (Pet* pet : pets)
        pet->on_start_turn();
}

void Team::order(size_t order[5]) {
    std::vector<Pet*> ordered_pets;
    for (size_t i=0; i<5; i++) {
        if (order[i] < pets.size())
            ordered_pets.push_back(pets[order[i]]);
    }

    pets = ordered_pets;
}

void Team::end_turn() {
    for (Pet* pet : pets)
        pet->on_end_turn();
    tmp_pets = pets;
}

void Team::reset() {
    for (Pet* pet : tmp_pets) {
        if (pet->is_tmp)
            delete pet;
    }

    tmp_pets = pets;
    for (Pet* pet : pets)
        pet->reset_stats();
}

void Team::add(Pet* pet) {
    _add(pet);
}

void Team::combine(size_t index, Pet* other_pet) {
    Pet* dst = pets[index];
    dst->combine(other_pet);
    delete other_pet;
}

void Team::combine(size_t src_index, size_t dst_index) {
    can_combine(src_index, dst_index);

    Pet* src = pets[src_index];
    combine(dst_index, src);
    pets.erase(pets.begin() + src_index);
}

int Team::sell(size_t index) {
    check_size("SELL_PET", index);

    Pet* pet = pets[index];
    int lvl = pet->get_level();
    pets.erase(pets.begin() + index);

    pet->on_sell();
    delete pet;

    return lvl;
}

void Team::summon(Pet* base_pet, Pet* new_pet) {
    spdlog::debug("New pet summoned: {}", new_pet->name);
    if (pets.size() >= 6) {
        spdlog::debug("Already 5 pets in team, aborting");
        delete new_pet;
        return;
    }

    std::vector<Pet*>* team_pets;
    if (in_fight)
        team_pets = &tmp_pets;
    else
        team_pets = &pets;
    auto it = std::find(team_pets->begin(), team_pets->end(), base_pet);
    team_pets->insert(it+1, new_pet);
}

void Team::faint(size_t index) {
    Pet* pet = pets[index];
    pet->on_faint();
    pets.erase(pets.begin() + index);
    delete pet;
}

void Team::give_object(size_t index, Object* obj) {
    check_size("GIVE_OBJECT", index);

    if (obj->type == ObjType::ITEM)
        pets[index]->equip_object(obj);

    pets[index]->on_object(obj);
    for (size_t i=0; i<pets.size(); i++) {
        if (i == index) continue;
        pets[i]->on_object_bought(index, obj);
    }
}

int Team::fight(Team* other_team) {
    in_fight = true;

    std::cout << "Team before attack:" << std::endl;
    while (!tmp_pets.empty() && !other_team->tmp_pets.empty()) {
        Pet* pet = tmp_pets.front();
        Pet* other_pet = other_team->tmp_pets.front();

        while (pet->is_alive() && other_pet->is_alive()) {
            if (pet->get_attack() > other_pet->get_attack()) {
                pet->attacks(other_pet);
                other_pet->attacks(pet);
            } else {
                other_pet->attacks(pet);
                pet->attacks(other_pet);
            }
        }

        if (!pet->is_alive()) {
            pet->on_faint();
            if (pet->is_tmp)
                delete pet;
            tmp_pets.erase(tmp_pets.begin());
        }
        if (!other_pet->is_alive()) {
            other_pet->on_faint();
            if (other_pet->is_tmp)
                delete other_pet;
            other_team->tmp_pets.erase(other_team->tmp_pets.begin());
        }

        disp_fight(other_team);
    }

    int output;
    if (tmp_pets.empty() && other_team->tmp_pets.empty())
        output = 0;
    else if (tmp_pets.empty())
        output = -1;
    else
        output = 1;

    if (output == 0)
        spdlog::debug("Draw !");
    else if (output == 1)
        spdlog::debug("Win !");
    else
        spdlog::debug("Loss...");

    in_fight = false;
    reset();
    other_team->reset();
    return output;
}

int Team::fight_step(Team* adv_team) {
    return -1;
}

void Team::disp_fight(Team const* const other_team) const {
    std::string pets_name;
    std::string stats;

    for (int i=tmp_pets.size()-1; i>=0; i--) {
        if (!tmp_pets[i]->is_alive()) continue;

        pets_name += utils::pad(tmp_pets[i]->name, 10);
        stats += utils::pad(tmp_pets[i]->disp_stats(), 10);
    }

    pets_name += "  ///  ";
    stats += "  ///  ";

    for (size_t i=0; i<other_team->tmp_pets.size(); i++) {
        if (!other_team->tmp_pets[i]->is_alive()) continue;

        pets_name += utils::pad(other_team->tmp_pets[i]->name, 10);
        stats += utils::pad(other_team->tmp_pets[i]->disp_stats(), 10);
    }

    std::cout << pets_name << "\n" << stats << std::endl;
}

std::tuple<int, std::string, std::string> Team::get_fight_str(Team* other_team) {
    in_fight = true;

    while (!tmp_pets.empty() && !other_team->tmp_pets.empty()) {
        Pet* pet = tmp_pets.front();
        Pet* other_pet = other_team->tmp_pets.front();

        while (pet->is_alive() && other_pet->is_alive()) {
            if (pet->get_attack() > other_pet->get_attack()) {
                pet->attacks(other_pet);
                other_pet->attacks(pet);
            } else {
                other_pet->attacks(pet);
                pet->attacks(other_pet);
            }
        }

        if (!pet->is_alive()) {
            pet->on_faint();
            if (pet->is_tmp)
                delete pet;
            tmp_pets.erase(tmp_pets.begin());
        }
        if (!other_pet->is_alive()) {
            other_pet->on_faint();
            if (other_pet->is_tmp)
                delete other_pet;
            other_team->tmp_pets.erase(other_team->tmp_pets.begin());
        }
    }

    int output;
    if (tmp_pets.empty() && other_team->tmp_pets.empty())
        output = 0;
    else if (tmp_pets.empty())
        output = -1;
    else
        output = 1;

    std::string team_str1 = serialize(true);
    std::string team_str2 = other_team->serialize(true);

    in_fight = false;
    reset();
    other_team->reset();
    return {output, team_str1, team_str2};
}

std::string Team::serialize(bool tmp) const {
    std::string team_str = std::to_string(turn) + " ";
    std::vector<Pet*> pets_list = pets;
    if (tmp)
        pets_list = tmp_pets;

    for (Pet* pet : pets_list) {
        if (!pet->is_alive()) continue;
        team_str += pet->serialize() + " ";
    }
    team_str.pop_back();
    return team_str;
}


void Team::load_teams() {
    std::ifstream team_file("data/saved_teams.txt");
    std::string team_str;
    while (getline(team_file, team_str)) {
        Team* new_team = Team::unserialize(team_str);
        Team::team_list[new_team->turn].push_back(new_team);
    }
}

void Team::check_size(std::string action, size_t index) const {
    if (index < pets.size()) return;
    throw InvalidAction("[" + action + "]: no pet in team at index " + std::to_string(index+1));
}

void Team::_add(Pet* pet) {
    pets.push_back(pet);
}
