#include <SFML/Graphics.hpp> //http://www.sfml-dev.org/tutorials/2.4
#include <vector>
#include <iostream> //For output to the terminal

#define game_W        700
#define game_H        500
#define tex_card_W    153
#define tex_card_H    200
#define card_W        76
#define card_H        100
#define tex_cursor_W  32
#define tex_cursor_H  32
#define cursor_W      32
#define cursor_H      32
#define margin        21  //Margin used in layout
#define card_peek     5   //Fraction of the card to show peeking from under overlapped cards
#define font_size     24


void pollInput (sf::RenderWindow& window, bool& mouse_down, bool& dragging)
{
    sf::Event event;
    while (window.pollEvent(event)) {
        switch (event.type) {
            case sf::Event::Closed:
                window.close();
                break;
            case sf::Event::MouseButtonPressed:
                mouse_down = true;
                break;
            case sf::Event::MouseButtonReleased:
                mouse_down = false;
                dragging = false;
                break;
            default: break;
        }
    }
}


//Properties of a deck
const bool _allow_drop = true, _disallow_drop = false;
const bool _stack = true, _pile = false;
const uint8_t _type_stock = 0, _type_draw = 1, _type_fountain = 2, _type_pile = 3, _type_drag = 5;

struct Card {
    uint8_t num;
    bool facedown = true;
};
bool operator==(const Card& l, const Card& r) { return (l.num == r.num); }
bool operator!=(const Card& l, const Card& r) { return (l.num != r.num); }
sf::Vector2f operator-(const sf::Vector2f& l, sf::Vector2i& r) { return sf::Vector2f(l.x - r.x, l.y - r.y); }
sf::Vector2f operator+(const sf::Vector2i& l, sf::Vector2f& r) { return sf::Vector2f(l.x + r.x, l.y + r.y); }

class Deck {
  public:
    sf::Vector2f pos;
    bool is_stack;
    bool allow_drop;
    std::vector<Card> cards;
    uint8_t type;
    
    Deck (sf::Vector2f, bool, bool, uint8_t);
};

Deck::Deck (sf::Vector2f pos_, bool is_stack_, bool allow_drop_, uint8_t type_)
{
    pos = pos_;
    is_stack = is_stack_;
    allow_drop = allow_drop_;
    type = type_;
}



int main ()
{
  //Create window
    sf::RenderWindow window (sf::VideoMode(game_W, game_H), "Panfield");
    sf::VideoMode desktop = sf::VideoMode::getDesktopMode();
    window.setPosition({ (int)(desktop.width/2) - (game_W/2), (int)(desktop.height/2) - (game_H/2) });
    window.setMouseCursorVisible(false);
    
  //Create textures and sprites
    //Textures
    sf::Texture tex_cursor;
    sf::Texture tex_table;
    sf::Texture tex_card;
    sf::Font font_arial;
    tex_table.loadFromFile("media/table.jpg");
    tex_cursor.loadFromFile("media/cursors.png");
    tex_card.loadFromFile("media/deck.png");
    font_arial.loadFromFile("media/arial.ttf");
    //Shapes & objects
    sf::RectangleShape spr_cursor ({ cursor_W, cursor_H });
    sf::RectangleShape spr_table ({ game_W, game_H });
    sf::RectangleShape spr_card ({ card_W, card_H });
    sf::RectangleShape spr_hole ({ card_W + margin, card_H + margin});
    sf::Text spr_text;
    //Settings
    tex_table.setRepeated(true);
    spr_table.setTexture(&tex_table);
    spr_table.setTextureRect({ 0, 0, game_W, game_H });
    spr_cursor.setTexture(&tex_cursor);
    spr_card.setTexture(&tex_card);
    spr_card.setOutlineThickness(1);
    spr_card.setOutlineColor(sf::Color(0, 0, 0));
    spr_hole.setFillColor(sf::Color(64, 0, 0, 128));
    spr_hole.setOutlineThickness(2);
    spr_hole.setOutlineColor(sf::Color(0, 0, 0));
    spr_text.setFont(font_arial);
    spr_text.setFillColor(sf::Color::Black);
    spr_text.setCharacterSize(font_size);
    spr_text.setPosition(margin, game_H - font_size - margin);

  //Prepare common variables
    const uint16_t fountains_X = game_W - (card_W + margin) * 4;
    const uint16_t piles_Y = card_H * 1.5;
    bool mouse_down = false, dragging = false;
    sf::Clock game_clock;
    bool paused = false;
    uint32_t pause_start = 0, pause_adjust = 0;
    uint8_t cards_in_fountain = 0;
    std::string previous_text;


  //Prepare common lambdas
    
    //Lambda to calculate fountain display X
    auto fountainPos = [fountains_X] (uint8_t f) -> sf::Vector2f {
        return sf::Vector2f (fountains_X + f * (card_W + margin), margin);
    };
    
    //Lambda to calculate pile display X
    auto pilePos = [piles_Y] (uint8_t p) -> sf::Vector2f {
        return sf::Vector2f ((p * card_W) + (margin * (p + 1)), piles_Y);
    };
    
    //Lambda to select correct card texture
    auto selectCardTexture = [&spr_card] (Card card) {
        if (card.facedown) {
            spr_card.setTextureRect({ 0, tex_card_H * 4, tex_card_W, tex_card_H });
        } else {
            uint8_t cX = card.num % 13;
            uint8_t cY = card.num / 13;
            spr_card.setTextureRect({ tex_card_W * cX, tex_card_H * cY, tex_card_W, tex_card_H });
        }
    };

//Start game logic
    
  //Setup resources
    //Decks
    std::vector<Deck*> decks;
      //Stock
    Deck* deck_stock = new Deck (sf::Vector2f (margin, margin), _stack, _disallow_drop, _type_stock);
    decks.push_back(deck_stock);
      //Draw
    Deck* deck_draw = new Deck (sf::Vector2f (margin*2 + card_W, margin), _stack, _disallow_drop, _type_draw);
    decks.push_back(deck_draw);
      //Fountains
    std::vector<Deck*> deck_fountains;
    for (uint8_t f = 0; f < 4; ++f) {
        deck_fountains.push_back(new Deck (fountainPos(f), _stack, _allow_drop, _type_fountain));
        decks.push_back(deck_fountains.back());
    }
      //Piles
    std::vector<Deck*> deck_piles;
    for (uint8_t p = 0; p < 7; ++p) {
        deck_piles.push_back(new Deck (pilePos(p), _pile, _allow_drop, _type_pile));
        decks.push_back(deck_piles.back());
    }
    
    //Lambda to handle dragging, moving cards into the drag deck
    Deck* deck_drag = new Deck(sf::Vector2f(0, 0), _pile, _disallow_drop, _type_drag); //Cards being dragged
    decks.push_back(deck_drag);
    Deck* deck_drag_from; //Pile drag originated from
    Deck* deck_drop_to; //Reported pile which card can be dropped onto
    deck_drag_from = deck_drop_to = nullptr;
    sf::Vector2f drag_offset;
    auto startDrag = [&deck_drag, &deck_drag_from, &spr_card, &drag_offset] (Deck& deck_parent, Card card, auto mouse_pos) {
        deck_drag_from = &deck_parent;
      //Find the card in the vector
        for (uint8_t i = 0, ilen = deck_drag_from->cards.size(); i < ilen; ++i) {
            if (deck_drag_from->cards[i] == card) {
              //Record mouse offset
                drag_offset = spr_card.getPosition() - mouse_pos;
              //Cycle through the pile until reaching the end of the pile
                for (uint8_t c = i, clen = deck_drag_from->cards.size(); c < clen; ++c) {    
                  //Move the card into the drag vector
                    deck_drag->cards.push_back(deck_drag_from->cards[c]);
                }
              //Remove dragged cards from the pile
                for (uint8_t c = i, clen = deck_drag_from->cards.size(); c < clen; ++c) {
                    deck_drag_from->cards.pop_back();
                }
                break;
            }
        }
    };
    
    //Lambda to determine if one card can be dropped onto another
    auto canDrop = [] (Card drop, Card to, bool for_fountain = false) -> bool {
        uint8_t drag_N = drop.num;
        uint8_t card_N = to.num;
        if (for_fountain) { return (drag_N == card_N + 1); }
        if (drag_N / 13 % 2 != card_N / 13 % 2) {
            return card_N % 13 == (drag_N % 13) + 1;
        } else {
            return false;
        }
    };
    
    //Lambda to draw 1 card from stock
    auto drawFromStock = [&deck_stock, &deck_draw] () {
        if (deck_stock->cards.size()) {
            deck_draw->cards.push_back(deck_stock->cards.back());
            deck_draw->cards.back().facedown = false;
            deck_stock->cards.pop_back();
        } else { //Refill stock
            deck_stock->cards.insert(deck_stock->cards.end(), deck_draw->cards.begin(), deck_draw->cards.end());
            std::vector<Card>().swap(deck_draw->cards);
            //Reverse
            std::reverse(deck_stock->cards.begin(), deck_stock->cards.end());
        }
    };

    
  //Deal decks
    Card card;
    for (uint8_t i = 0; i < 52; ++i) {
        card.num = i;
        deck_stock->cards.push_back(card);
    }
    //Randomise stock
    srand(time(NULL));
    std::random_shuffle(deck_stock->cards.begin(), deck_stock->cards.end());
    //Deal piles
    for (uint8_t p = 0; p < 7; ++p) {
        for (uint8_t c = 0, clen = p + 1; c < clen; ++c) {
            Card to_move = deck_stock->cards.back();
            deck_stock->cards.pop_back();
            to_move.facedown = !(c == p);
            deck_piles[p]->cards.push_back(to_move);
        }
    }
    
    
      
    
//Begin game-loop
    while (window.isOpen()) {

      //Sleep thread
        sf::sleep(sf::milliseconds(16));
        
      //Poll input
        bool was_dragging = dragging;
        pollInput(window, mouse_down, dragging);      
        
      //Check pause status
        if (!window.hasFocus() && !paused) {
            paused = true;
            pause_start = (int)game_clock.getElapsedTime().asSeconds();
            window.setMouseCursorVisible(true);
        } else if (paused && window.hasFocus()) {
            paused = false;
          //Calculate the time between now and the pause start
            pause_adjust += (int)game_clock.getElapsedTime().asSeconds() - pause_start;
            window.setMouseCursorVisible(false);
        }
        if (paused) { continue; }
    
    //Logic
    
        auto mouse_pos = sf::Mouse::getPosition(window);
        bool hovered = false;  
        
        
    //Drawing
      //Clear
        window.clear();
        
      //Draw table
        window.draw(spr_table);
        
        sf::Rect<float> drag_rect (mouse_pos.x + drag_offset.x, mouse_pos.y + drag_offset.y, card_W, card_H);
        
      //Draw decks
        int8_t highlight_remaining_deck = -1;
        if (dragging) { deck_drop_to = nullptr; }
        for (uint8_t d = 0, dlen = decks.size(); d < dlen; ++d) {
            bool is_stock = (decks[d]->type == _type_stock);
            bool is_fountain = (decks[d]->type == _type_fountain);
            uint8_t clen = decks[d]->cards.size();
            uint8_t c = 0;
          //If a stack: draw a hole, select only the last two cards be drawn
            spr_hole.setPosition(decks[d]->pos); //Though it may not be drawn, it's still used for drop detection
            spr_hole.move(-margin/2, -margin/2);
            if (decks[d]->is_stack) {
                window.draw(spr_hole);
                c = (clen >= 2 ? clen - 2 : 0);
            }
          //If stock: check if the mouse is over us, or trying to draw from stock
            if (is_stock) {
                bool over_us = spr_hole.getGlobalBounds().contains(mouse_pos.x, mouse_pos.y);
                hovered |= over_us;
                if (over_us && mouse_down) {
                    mouse_down = false;
                    drawFromStock();
                }
            }
          //Draw cards
            bool being_dragged = !(decks[d]->pos.x + decks[d]->pos.y);
            for (; c < clen; ++c) {
                Card this_card = decks[d]->cards[c];
                bool is_top = (c == clen - 1);
                if (decks[d]->is_stack) {
                    spr_card.setPosition(decks[d]->pos);
                    spr_card.setFillColor(sf::Color(255, 255, 255));
                } else {
                    spr_card.setPosition(decks[d]->pos.x, decks[d]->pos.y + (c*card_H / card_peek));
                    if (being_dragged) { spr_card.move(mouse_pos + drag_offset); }
                  //Calculate gradient
                    uint8_t grad = 255 - ((clen - c) * 16);
                    spr_card.setFillColor(sf::Color(grad, grad, grad));
                }
                if (is_stock) { this_card.facedown = true; }
                selectCardTexture(this_card);
              //Check mouse
                if (!this_card.facedown && (decks[d]->is_stack ? is_top : true)) {
                    auto bounds = spr_card.getGlobalBounds();
                    if (c != clen - 1) { bounds.height /= card_peek; }
                    bool over_us = bounds.contains(mouse_pos.x, mouse_pos.y);
                    hovered |= over_us;
                  //If we're being hovered over: highlight, maybe drag, maybe place
                    if ((over_us && !dragging) || (highlight_remaining_deck == d)) {
                        highlight_remaining_deck = d;
                        spr_card.setFillColor(sf::Color(255, 200, 200));
                      //Check if starting to drag the card, or right click to place in fountain or piles
                        if (over_us && mouse_down && !dragging) {
                            startDrag(*decks.at(d), this_card, mouse_pos);
                            dragging = true;
                            if (sf::Mouse::isButtonPressed(sf::Mouse::Right)) {
                              //Try moving into the fountains
                                bool fountains_success = false;
                                if (deck_drag->cards.size() == 1) {
                                    bool is_ace = !(this_card.num % 13);
                                    for (uint8_t f = 0; f < 4; ++f) {
                                        if (is_ace) { //If an ace, place it on the first empty fountain
                                            fountains_success = !deck_fountains[f]->cards.size();
                                        } else { //If a normal card, find its correct fountain
                                            if (deck_fountains[f]->cards.size()) { fountains_success = (deck_fountains[f]->cards.back().num / 13 == this_card.num / 13); }
                                        }
                                        if (fountains_success && (is_ace ? true : fountains_success = canDrop(deck_drag->cards.front(), deck_fountains[f]->cards.back(), true))) {
                                            deck_drop_to = deck_fountains[f];
                                            break;
                                        }
                                    }
                                }
                              //Try moving into the piles
                                bool success = fountains_success;
                                if (!fountains_success) {
                                  //Search the piles
                                    for (uint8_t p = 0; p < 7; ++p) {
                                        if (!deck_piles[p]->cards.size()                //
                                            && deck_drag->cards.front().num % 13 == 12  // A king to an empty pile
                                            ||
                                            !deck_piles[p]->cards.back().facedown                             //
                                            && deck_piles[p]->cards.back() != deck_drag_from->cards.back()    //
                                            && canDrop(deck_drag->cards.front(), deck_piles[p]->cards.back()) // Normal
                                            ) {
                                            deck_drop_to = deck_piles[p];
                                            success = true;
                                            break;
                                        }
                                    }
                                }
                                if (success) {
                                    mouse_down = false;  //
                                    was_dragging = true; //
                                    dragging = false;    // Elicit a drag-n-drop onto the fountain
                                } else {
                                    deck_drop_to = nullptr;
                                }
                            }
                        }
                    }
                  //Check if hovering to drop
                    if (decks[d]->allow_drop && dragging && is_top) {
                        over_us = bounds.intersects(drag_rect);
                        if (over_us && canDrop(deck_drag->cards.front(), this_card, is_fountain) && (is_fountain ? deck_drag->cards.size() == 1 : true)) {
                            deck_drop_to = decks[d];
                            spr_card.setFillColor(sf::Color(128, 128, 255));
                        }
                    }
                }
              //Draw
                window.draw(spr_card);
            }
          //Check if hovering over an empty deck for drop
            if (dragging && decks[d]->allow_drop && !decks[d]->cards.size()) {
                bool over_us = spr_hole.getGlobalBounds().intersects(drag_rect);
                bool is_king_onto_pile = (deck_drag->cards.front().num % 13 == 12);
                bool is_ace_onto_fountain = is_fountain && (deck_drag->cards[0].num % 13 == 0);
                if (over_us && (is_king_onto_pile || is_ace_onto_fountain)) {
                    if (!deck_drop_to) { deck_drop_to = decks[d]; }
                }
            }
        }
        
      //Draw info text
        uint32_t now = ((int)game_clock.getElapsedTime().asSeconds() - pause_adjust);
        uint8_t seconds = now % 60;
        uint8_t minutes = now / 60;
        previous_text = ((cards_in_fountain >= 52) ? previous_text : (minutes < 10 ? "0" : "") + std::to_string(minutes) + ":" + (seconds < 10 ? "0" : "") + std::to_string(seconds)
         + "  " + std::to_string((int)((float)cards_in_fountain / 52 * 100)) + "%");
        spr_text.setString(previous_text);
        window.draw(spr_text);

      //Draw cursor
        spr_cursor.setPosition(mouse_pos.x - cursor_W/2, mouse_pos.y - cursor_H/2);
        spr_cursor.setTextureRect({ tex_cursor_W * (hovered + dragging), 0, tex_cursor_W, tex_cursor_H });
        window.draw(spr_cursor);
      
      //Check if dropping
        if (was_dragging && !dragging) {
          //Check if our new location was droppable
            if (deck_drop_to) {
                deck_drop_to->cards.insert(deck_drop_to->cards.end(), deck_drag->cards.begin(), deck_drag->cards.end());
                if (deck_drop_to->type == _type_fountain) { ++cards_in_fountain; }
              //Face up last card
                deck_drag_from->cards.back().facedown = false;
            } else { //It was apparently not droppable
              //Return dragged items back to they hive
                deck_drag_from->cards.insert(deck_drag_from->cards.end(), deck_drag->cards.begin(), deck_drag->cards.end());
            }
          //Clear the drag vectors
            std::vector<Card>().swap(deck_drag->cards);
            deck_drag_from = deck_drop_to = nullptr;
        }
        
      //Display
        window.display();
    }
    return 0;
}
