/*
    ATM Simulation System
    Author: Team Benzene
    Description: Full-featured ATM UI using SFML, supporting login, PIN, cash withdrawal, transfer, payments, PIN change, etc.
    Features:
    - User authentication (ID & PIN)
    - Withdrawals (quick, custom)
    - Transfers (to other user IDs)
    - Payments (mobile topup, utility bills)
    - PIN change
    - Success/error feedback (image, sound)
    - Persistent accounts file
    - Proper error return (returns to the originating screen after error)
*/

#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <SFML/Audio.hpp>
#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <memory>
#include <openssl/sha.h>

//Accoount Class
// Account holds userID, hashed PIN, and balance.
class Account {
public:
    std::string userID;
    std::string pinHash; // SHA256 hash
    double balance;

    Account(const std::string& id, const std::string& hash, double bal)
        : userID(id), pinHash(hash), balance(bal) {
    }
};

/*------------------ Utility Functions -------------------*/
namespace Util {
    //Returns SHA256 hash of input string.
     
    std::string sha256(const std::string& input) {
        unsigned char hash[SHA256_DIGEST_LENGTH];
        SHA256(reinterpret_cast<const unsigned char*>(input.c_str()), input.size(), hash);
        std::stringstream ss;
        for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i)
            ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
        return ss.str();
    }
}

// ATM Resources 
// Holds fonts, textures, sounds, colors used in ATM UI.
 
struct ATMResources {
    sf::Font font;
    sf::Texture bgTexture;
    sf::Sprite bgSprite;

    // Success/Error Textures & Sprites
    sf::Texture cashSuccessTexture, pinSuccessTexture, topupSuccessTexture, billSuccessTexture, transferSuccessTexture, errorTexture;
    sf::Sprite cashSuccessSprite, pinSuccessSprite, topupSuccessSprite, billSuccessSprite, transferSuccessSprite, errorSprite;

    // Sounds: 0=Cash, 1=Pin, 2=Topup, 3=Bill, 4=Transfer, 5=Error
    sf::SoundBuffer soundBuffers[6];
    sf::Sound sounds[6];

    // Color scheme
    sf::Color mainBG = sf::Color(32, 42, 68);
    sf::Color menuBG = sf::Color(44, 60, 93);
    sf::Color menuSelect = sf::Color(0, 120, 230);
    sf::Color menuNormal = sf::Color(0, 174, 239);
    sf::Color heading = sf::Color(255, 217, 0);
    sf::Color field = sf::Color::White;
    sf::Color errorC = sf::Color(255, 80, 80);
    sf::Color successC = sf::Color(60, 200, 70);
    sf::Color infoC = sf::Color(180, 180, 180);

    // Loads all resources from "assets" folder.
    bool load() {
        // Font
        if (!font.loadFromFile("assets/arial.ttf")) {
            std::cerr << "Error loading font\n";
            return false;
        }
        // Background
        if (bgTexture.loadFromFile("assets/temp.jpg")) {
            bgSprite.setTexture(bgTexture);
            bgSprite.setScale(650.f / bgTexture.getSize().x, 420.f / bgTexture.getSize().y);
        }
        else {
            std::cerr << "Error loading background image\n";
        }
        // Success Images (INCREASED SIZE: 220px)
        auto loadSuccessImage = [&](sf::Texture& tex, sf::Sprite& spr, const char* file) {
            if (tex.loadFromFile(file)) {
                spr.setTexture(tex);
                float scale = 220.f / tex.getSize().x; // <-- CHANGE: increased image size
                spr.setScale(scale, scale);
                spr.setPosition(650.f / 2 - 110, 120);  // Centered, slightly higher for larger images
            }
            };
        loadSuccessImage(cashSuccessTexture, cashSuccessSprite, "assets/cash_success.png");
        loadSuccessImage(pinSuccessTexture, pinSuccessSprite, "assets/pin_success.png");
        loadSuccessImage(topupSuccessTexture, topupSuccessSprite, "assets/topup_success.png");
        loadSuccessImage(billSuccessTexture, billSuccessSprite, "assets/bill_success.png");
        loadSuccessImage(transferSuccessTexture, transferSuccessSprite, "assets/transfer_success.png");
        if (errorTexture.loadFromFile("assets/error.png")) {
            errorSprite.setTexture(errorTexture);
            float scale = 120.f / errorTexture.getSize().x;
            errorSprite.setScale(scale, scale);
            errorSprite.setPosition(650.f / 2 - 60, 110);
        }
        else {
            std::cerr << "Error loading error image\n";
        }
        // Sounds
        const char* soundFiles[6] = {
            "assets/cash.wav", "assets/pin.wav", "assets/topup.wav",
            "assets/bill.wav", "assets/transfer.wav", "assets/error.wav"
        };
        for (int i = 0; i < 6; ++i) {
            if (soundBuffers[i].loadFromFile(soundFiles[i])) {
                sounds[i].setBuffer(soundBuffers[i]);
            }
            else {
                std::cerr << "Error loading sound file: " << soundFiles[i] << std::endl;
            }
        }
        return true;
    }
};

//Abstract Screen Class
// Base class for screens. All screens implement handleEvent, draw, onEnter.
 
class ATM; 

class ScreenBase {
public:
    virtual ~ScreenBase() = default;
    virtual void handleEvent(ATM& atm, const sf::Event& event) = 0;
    virtual void draw(ATM& atm) = 0;
    virtual void onEnter(ATM& atm) {}
};

using ScreenPtr = std::unique_ptr<ScreenBase>;

// ATM Main Class
// ATM main controller class. Holds state, manages screens, accounts, UI utility methods.
class ATM {
public:
    // Transaction/Feedback types
    enum SuccessType { Cash, Pin, Topup, Bill, Transfer, Error, None };

    // SFML UI
    sf::RenderWindow window;
    sf::View view;
    ATMResources resources;

    // Account state
    std::vector<Account> accounts;
    Account* currentAccount = nullptr;

    // UI/Screen state
    SuccessType lastSuccessType = None;
    std::string errorMsg, successMsg;
    int menuSelection = 0, paymentSelection = 0, otherMenuSelection = 0, quickDrawSelection = -1;
    std::string loginUserInput, pinInput, withdrawalInput, transferAccountInput, transferAmountInput;
    std::string mobileTopupInput, billAmountInput, customerNoInput, newPinInput;

    // Focus (for transfer screen input box switching)
    bool transferAccountInputFocused = true;

    // Menu items
    const std::vector<std::string> menuItems{ "Quick Cash", "Cash Withdrawal", "Payment/Purchase", "Other Transactions", "Exit" };
    const std::vector<std::string> paymentItems{ "Mobile Topup", "Electricity Bill", "Water Bill", "Back" };
    const std::vector<std::string> otherItems{ "Balance Inquiry", "Transfer", "Change PIN", "Back" };
    const std::vector<int> quickDrawAmounts{ 1000, 2000, 5000, 10000, 15000, 20000, 25000, 50000 };
    std::string accountFile = "assets/accounts.txt";

    // Screen management
    std::map<std::string, ScreenPtr> screens;
    std::string currentScreenKey;
    std::string errorReturnScreen = ""; 

    //Constructor
    ATM() : window(sf::VideoMode(650, 420), "ATM Simulation System") {
        window.setKeyRepeatEnabled(true);
        view.setSize(650.f, 420.f);
        view.setCenter(650.f / 2.f, 420.f / 2.f);
        if (!resources.load()) std::cerr << "Error loading resources\n";
        loadAccountsFromFile();
        setupScreens();
        switchScreen("login");
    }

    //Screen Management
    void setupScreens();
    // Switch to a different screen by key.
    
    void switchScreen(const std::string& key) {
        if (key != "error") errorMsg.clear();
        if (key != "success") successMsg.clear();
        if (key != "success") lastSuccessType = None;
        currentScreenKey = key;
        if (screens.count(key))
            screens[key]->onEnter(*this);
    }

	// Show error and which screen to return to.    
     
    void showError(const std::string& msg, const std::string& returnScreen = "") {
        errorMsg = msg;
        lastSuccessType = Error;
        playSound(Error);
        errorReturnScreen = returnScreen;
        switchScreen("error");
    }

    //Account Load/Save
    // Loads all accounts from file.
    void loadAccountsFromFile() {
        accounts.clear();
        std::ifstream file(accountFile);
        if (!file.is_open()) {
            std::cerr << "Error: Could not open accounts.txt\n";
            return;
        }
        std::string line;
        while (std::getline(file, line)) {
            std::istringstream iss(line);
            std::string userID, pin;
            double balance;
            if (iss >> userID >> pin >> balance) {
                accounts.emplace_back(userID, pin, balance);
            }
        }
        file.close();
    }
	//Savwall accounts to file. 
    void saveAccountsToFile() {
        std::ofstream fout(accountFile, std::ios::trunc);
        for (const auto& acc : accounts) {
            fout << acc.userID << " " << acc.pinHash << " " << acc.balance << "\n";
        }
    }

	//Sound Management
	//plays sound based on SuccessType.
    void playSound(SuccessType type) {
        int idx = int(type);
        if (idx >= 0 && idx < 6 && resources.soundBuffers[idx].getSampleCount() > 0)
            resources.sounds[idx].play();
    }

    /*------------------ UI Utility -------------------*/
	// Draws centered text at specified Y position.
    void drawCenteredText(const std::string& text, float y, int size = 28, sf::Color color = sf::Color::White) {
        sf::Text t(text, resources.font, size);
        t.setFillColor(color);
        sf::FloatRect bounds = t.getLocalBounds();
        t.setOrigin(bounds.width / 2, bounds.height / 2);
        t.setPosition(650.f / 2.f, y);
        window.draw(t);
    }
	//Draws a single input box.
    void drawInputBox(const std::string& input, float x, float y, float w = 220, float h = 40, bool mask = false) {
        sf::RectangleShape box(sf::Vector2f(w, h));
        box.setPosition(x, y);
        box.setFillColor(resources.field);
        box.setOutlineThickness(2);
        box.setOutlineColor(resources.menuNormal);
        window.draw(box);
        std::string display = mask ? std::string(input.size(), '*') : input;
        sf::Text txt(display, resources.font, 26);
        txt.setFillColor(sf::Color::Black);
        sf::FloatRect textBounds = txt.getLocalBounds();
        while (textBounds.width > w - 24 && !display.empty()) {
            display = display.substr(1);
            txt.setString(display);
            textBounds = txt.getLocalBounds();
        }
        txt.setPosition(x + 12, y + 5);
        window.draw(txt);
    }
	//Draws Menu boxes for a list of items.
    void drawMenuBoxes(const std::vector<std::string>& items, int selection, int btnWidth, int btnHeight, int spacingY, int startY, int fontSize = 20) {
        int half = (items.size() + 1) / 2;
        int leftX = 36;
        int rightX = 650 - btnWidth - 36;
        for (int i = 0; i < half; ++i) {
            sf::RectangleShape btn(sf::Vector2f(btnWidth, btnHeight));
            btn.setPosition(leftX, startY + i * (btnHeight + spacingY));
            btn.setFillColor((selection == i) ? resources.menuSelect : resources.menuNormal);
            btn.setOutlineThickness(2);
            btn.setOutlineColor(sf::Color::White);
            window.draw(btn);
            sf::Text txt(items[i], resources.font, fontSize);
            txt.setFillColor(sf::Color::White);
            float maxWidth = btnWidth - 40;
            sf::FloatRect bounds = txt.getLocalBounds();
            while (bounds.width > maxWidth && !items[i].empty()) {
                txt.setString(txt.getString().substring(0, txt.getString().getSize() - 1));
                bounds = txt.getLocalBounds();
            }
            txt.setPosition(leftX + 20, startY + i * (btnHeight + spacingY) + 7);
            window.draw(txt);
        }
        for (size_t i = half; i < items.size(); ++i) {
            int idx = i - half;
            sf::RectangleShape btn(sf::Vector2f(btnWidth, btnHeight));
            btn.setPosition(rightX, startY + idx * (btnHeight + spacingY));
            btn.setFillColor((selection == i) ? resources.menuSelect : resources.menuNormal);
            btn.setOutlineThickness(2);
            btn.setOutlineColor(sf::Color::White);
            window.draw(btn);
            sf::Text txt(items[i], resources.font, fontSize);
            txt.setFillColor(sf::Color::White);
            float maxWidth = btnWidth - 40;
            sf::FloatRect bounds = txt.getLocalBounds();
            while (bounds.width > maxWidth && !items[i].empty()) {
                txt.setString(txt.getString().substring(0, txt.getString().getSize() - 1));
                bounds = txt.getLocalBounds();
            }
            txt.setPosition(rightX + 20, startY + idx * (btnHeight + spacingY) + 7);
            window.draw(txt);
        }
    }
	// Get clicked menu box index based on mouse position.
    int getMenuBoxClicked(const std::vector<std::string>& items, int btnWidth, int btnHeight, int spacingY, int startY, sf::Vector2f mousePos) {
        int half = (items.size() + 1) / 2;
        int leftX = 36;
        int rightX = 650 - btnWidth - 36;
        for (int i = 0; i < half; ++i) {
            sf::FloatRect r(leftX, startY + i * (btnHeight + spacingY), btnWidth, btnHeight);
            if (r.contains(mousePos)) return i;
        }
        for (size_t i = half; i < items.size(); ++i) {
            int idx = i - half;
            sf::FloatRect r(rightX, startY + idx * (btnHeight + spacingY), btnWidth, btnHeight);
            if (r.contains(mousePos)) return i;
        }
        return -1;
    }

	//Man Event Loop
	//Runs the main event loop, handles events, draws current screen.
    void run() {
        while (window.isOpen()) {
            sf::Event event;
            while (window.pollEvent(event)) {
                if (event.type == sf::Event::Closed)
                    window.close();
                if (screens.count(currentScreenKey))
                    screens[currentScreenKey]->handleEvent(*this, event);
            }
            window.clear(resources.mainBG);
            window.setView(view);
            if (resources.bgTexture.getSize().x > 0)
                window.draw(resources.bgSprite);
            if (screens.count(currentScreenKey))
                screens[currentScreenKey]->draw(*this);
            window.display();
        }
    }
};

//Screen Implementations

//LoginScreen: User enters ID to insert card.
class LoginScreen : public ScreenBase {
public:
    void draw(ATM& atm) override {
        atm.drawCenteredText("ATM SIMULATION SYSTEM", 60, 40, atm.resources.heading);
        atm.drawCenteredText("Enter User ID to Insert Card:", 120, 24, sf::Color::White);
        atm.drawInputBox(atm.loginUserInput, 650.f / 2 - 110, 150, 220, 44, false);
        atm.drawCenteredText("\nPress ENTER to continue", 210, 18, atm.resources.infoC);
        if (!atm.errorMsg.empty())
            atm.drawCenteredText(atm.errorMsg, 240, 16, atm.resources.errorC);
    }
    void handleEvent(ATM& atm, const sf::Event& event) override {
        if (event.type == sf::Event::TextEntered) {
            if (event.text.unicode < 128) {
                char ch = static_cast<char>(event.text.unicode);
                if (isdigit(ch) && atm.loginUserInput.size() < 12)
                    atm.loginUserInput += ch;
                else if (ch == '\b' && !atm.loginUserInput.empty())
                    atm.loginUserInput.pop_back();
                else if (ch == '\r') {
                    if (atm.loginUserInput.empty()) return;
                    for (auto& acc : atm.accounts) {
                        if (acc.userID == atm.loginUserInput) {
                            atm.currentAccount = &acc;
                            atm.switchScreen("pin");
                            return;
                        }
                    }
                    atm.showError("User ID not found!", "login");
                }
            }
        }
    }
    void onEnter(ATM& atm) override {
        atm.loginUserInput.clear(); atm.pinInput.clear(); atm.currentAccount = nullptr;
    }
};

//PinScreen: User enters PIN to authenticate.
class PinScreen : public ScreenBase {
public:
    void draw(ATM& atm) override {
        atm.drawCenteredText("Enter PIN (6 digits):", 120, 28, sf::Color::White);
        atm.drawInputBox(atm.pinInput, 650.f / 2 - 110, 150, 220, 44, true);
        atm.drawCenteredText("Press ENTER to continue", 210, 18, atm.resources.infoC);
        if (!atm.errorMsg.empty())
            atm.drawCenteredText(atm.errorMsg, 240, 16, atm.resources.errorC);
    }
    void handleEvent(ATM& atm, const sf::Event& event) override {
        if (event.type == sf::Event::TextEntered) {
            if (event.text.unicode < 128) {
                char ch = static_cast<char>(event.text.unicode);
                if (isdigit(ch) && atm.pinInput.size() < 6)
                    atm.pinInput += ch;
                else if (ch == '\b' && !atm.pinInput.empty())
                    atm.pinInput.pop_back();
                else if (ch == '\r') {
                    if (atm.pinInput.size() != 6) return;
                    if (atm.currentAccount && atm.currentAccount->pinHash == Util::sha256(atm.pinInput))
                        atm.switchScreen("mainmenu");
                    else {
                        atm.showError("Invalid PIN!", "pin");
                    }
                }
            }
        }
    }
    void onEnter(ATM& atm) override { atm.pinInput.clear(); }
};

//MainMenuScreen: Main menu with options.
class MainMenuScreen : public ScreenBase {
public:
    void draw(ATM& atm) override {
        atm.drawCenteredText("MAIN MENU", 40, 38, atm.resources.heading);
        atm.drawMenuBoxes(atm.menuItems, atm.menuSelection, 260, 45, 18, 90, 24);
        atm.drawCenteredText("Use UP/DOWN arrows or click, ENTER to select", 320, 17, atm.resources.infoC);
    }
    void handleEvent(ATM& atm, const sf::Event& event) override {
        if (event.type == sf::Event::KeyPressed) {
            if (event.key.code == sf::Keyboard::Down)
                atm.menuSelection = (atm.menuSelection + 1) % atm.menuItems.size();
            else if (event.key.code == sf::Keyboard::Up)
                atm.menuSelection = (atm.menuSelection + atm.menuItems.size() - 1) % atm.menuItems.size();
            else if (event.key.code == sf::Keyboard::Enter) {
                openMenu(atm, atm.menuSelection);
            }
        }
        else if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
            sf::Vector2i pixelPos = { event.mouseButton.x, event.mouseButton.y };
            sf::Vector2f worldPos = atm.window.mapPixelToCoords(pixelPos);
            int clicked = atm.getMenuBoxClicked(atm.menuItems, 260, 45, 18, 90, worldPos);
            if (clicked != -1) {
                atm.menuSelection = clicked;
                openMenu(atm, clicked);
            }
        }
    }
    static void openMenu(ATM& atm, int idx) {
        switch (idx) {
        case 0: atm.quickDrawSelection = -1; atm.switchScreen("quickdraw"); break;
        case 1: atm.switchScreen("withdrawal"); break;
        case 2: atm.switchScreen("paymentmenu"); break;
        case 3: atm.switchScreen("othermenu"); break;
        case 4: atm.window.close(); break;
        }
    }
    void onEnter(ATM& atm) override { atm.menuSelection = 0; }
};

//QuickDrawScreen: Quick cash withdrawal options.
class QuickDrawScreen : public ScreenBase {
public:
    void draw(ATM& atm) override {
        atm.drawCenteredText("QUICK CASH", 40, 36, atm.resources.heading);
        int btnWidth = 150, btnHeight = 45, spacingY = 20, startY = 90;
        int leftX = 30;
        int rightX = 650 - btnWidth - 30;
        int centerY = startY + 2 * (btnHeight + spacingY);

        for (int i = 0; i < 4; ++i) {
            sf::RectangleShape leftBtn(sf::Vector2f(btnWidth, btnHeight));
            leftBtn.setPosition(leftX, startY + i * (btnHeight + spacingY));
            leftBtn.setFillColor((atm.quickDrawSelection == i) ? atm.resources.menuSelect : atm.resources.menuNormal);
            leftBtn.setOutlineThickness(3);
            leftBtn.setOutlineColor(sf::Color::White);
            atm.window.draw(leftBtn);

            sf::Text leftText(std::to_string(atm.quickDrawAmounts[i]), atm.resources.font, 22);
            leftText.setFillColor(sf::Color::White);
            leftText.setPosition(leftX + 40, startY + i * (btnHeight + spacingY) + 10);
            atm.window.draw(leftText);
        }
        for (int i = 0; i < 4; ++i) {
            sf::RectangleShape rightBtn(sf::Vector2f(btnWidth, btnHeight));
            rightBtn.setPosition(rightX, startY + i * (btnHeight + spacingY));
            rightBtn.setFillColor((atm.quickDrawSelection == i + 4) ? atm.resources.menuSelect : atm.resources.menuNormal);
            rightBtn.setOutlineThickness(3);
            rightBtn.setOutlineColor(sf::Color::White);
            atm.window.draw(rightBtn);

            sf::Text rightText(std::to_string(atm.quickDrawAmounts[i + 4]), atm.resources.font, 22);
            rightText.setFillColor(sf::Color::White);
            rightText.setPosition(rightX + 40, startY + i * (btnHeight + spacingY) + 10);
            atm.window.draw(rightText);
        }

        if (atm.quickDrawSelection >= 0 && atm.quickDrawSelection < 8) {
            atm.drawCenteredText("Selected: Rs " + std::to_string(atm.quickDrawAmounts[atm.quickDrawSelection]), centerY, 26, atm.resources.heading);
        }
        else {
            atm.drawCenteredText("Select an amount", centerY, 22, sf::Color::White);
        }
        atm.drawCenteredText("Use arrow keys or click to select, Enter to withdraw, ESC for menu", 370, 18, atm.resources.infoC);
    }
    void handleEvent(ATM& atm, const sf::Event& event) override {
        if (event.type == sf::Event::KeyPressed) {
            if (event.key.code == sf::Keyboard::Left) {
                if (atm.quickDrawSelection >= 4) atm.quickDrawSelection -= 4;
            }
            else if (event.key.code == sf::Keyboard::Right) {
                if (atm.quickDrawSelection < 4 && atm.quickDrawSelection != -1) atm.quickDrawSelection += 4;
            }
            else if (event.key.code == sf::Keyboard::Up) {
                if (atm.quickDrawSelection == -1) atm.quickDrawSelection = 0;
                else if (atm.quickDrawSelection > 0) atm.quickDrawSelection -= 1;
            }
            else if (event.key.code == sf::Keyboard::Down) {
                if (atm.quickDrawSelection == -1) atm.quickDrawSelection = 0;
                else if (atm.quickDrawSelection < 7) atm.quickDrawSelection += 1;
            }
            else if (event.key.code == sf::Keyboard::Enter) {
                if (atm.quickDrawSelection >= 0 && atm.quickDrawSelection < 8) {
                    int amt = atm.quickDrawAmounts[atm.quickDrawSelection];
                    if (amt > atm.currentAccount->balance) {
                        atm.showError("Insufficient balance!", "quickdraw");
                    }
                    else {
                        atm.currentAccount->balance -= amt;
                        atm.saveAccountsToFile();
                        atm.successMsg = "Quick Draw successful! Rs " + std::to_string(amt) +
                            "\nRemaining balance: Rs " + std::to_string(int(atm.currentAccount->balance));
                        atm.lastSuccessType = ATM::Cash;
                        atm.playSound(atm.lastSuccessType);
                        atm.switchScreen("success");
                    }
                }
            }
            else if (event.key.code == sf::Keyboard::Escape) {
                atm.switchScreen("mainmenu");
            }
        }
        else if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
            sf::Vector2i pixelPos = { event.mouseButton.x, event.mouseButton.y };
            sf::Vector2f worldPos = atm.window.mapPixelToCoords(pixelPos);

            int btnWidth = 150, btnHeight = 45, spacingY = 20, startY = 90;
            int leftX = 30;
            int rightX = 650 - btnWidth - 30;

            for (int i = 0; i < 4; ++i) {
                sf::FloatRect r(leftX, startY + i * (btnHeight + spacingY), btnWidth, btnHeight);
                if (r.contains(worldPos)) atm.quickDrawSelection = i;
            }
            for (int i = 0; i < 4; ++i) {
                sf::FloatRect r(rightX, startY + i * (btnHeight + spacingY), btnWidth, btnHeight);
                if (r.contains(worldPos)) atm.quickDrawSelection = i + 4;
            }
        }
    }
    void onEnter(ATM& atm) override { atm.quickDrawSelection = -1; }
};

// WithdrawalScreen: User enters custom withdrawal amount.
class WithdrawalScreen : public ScreenBase {
public:
    void draw(ATM& atm) override {
        atm.drawCenteredText("CASH WITHDRAWAL", 40, 36, atm.resources.heading);
        atm.drawCenteredText("Enter Amount (multiple of 1000, Max 2500000):", 80, 20, sf::Color::White);
        float boxWidth = 220, boxHeight = 44;
        float boxX = 650.f / 2.f - boxWidth / 2;
        atm.drawInputBox(atm.withdrawalInput, boxX, 110, boxWidth, boxHeight, false);
        atm.drawCenteredText("Press ENTER to proceed, ESC for menu", 320, 17, atm.resources.infoC);
        if (!atm.errorMsg.empty())
            atm.drawCenteredText(atm.errorMsg, 360, 17, atm.resources.errorC);
    }
    void handleEvent(ATM& atm, const sf::Event& event) override {
        if (event.type == sf::Event::TextEntered) {
            if (event.text.unicode < 128) {
                char ch = static_cast<char>(event.text.unicode);
                if (isdigit(ch) && atm.withdrawalInput.size() < 8)
                    atm.withdrawalInput += ch;
                else if (ch == '\b' && !atm.withdrawalInput.empty())
                    atm.withdrawalInput.pop_back();
            }
        }
        else if (event.type == sf::Event::KeyPressed) {
            if (event.key.code == sf::Keyboard::Escape)
                atm.switchScreen("mainmenu");
            else if (event.key.code == sf::Keyboard::Enter) {
                if (atm.withdrawalInput.empty()) return;
                double amt = std::stod(atm.withdrawalInput);
                if (amt < 1000) { atm.showError("Minimum is 1000!", "withdrawal"); return; }
                if (amt > 2500000) { atm.showError("Max is 2,500,000!", "withdrawal"); return; }
                if (fmod(amt, 1000) != 0) { atm.showError("Must be multiple of 1000!", "withdrawal"); return; }
                if (amt > atm.currentAccount->balance) { atm.showError("Insufficient balance!", "withdrawal"); return; }
                atm.currentAccount->balance -= amt;
                atm.saveAccountsToFile();
                atm.successMsg = "Withdrawal successful! Amount: Rs " + std::to_string(int(amt)) +
                    "\nRemaining balance: Rs " + std::to_string(int(atm.currentAccount->balance));
                atm.lastSuccessType = ATM::Cash;
                atm.playSound(atm.lastSuccessType);
                atm.switchScreen("success");
            }
        }
    }
    void onEnter(ATM& atm) override { atm.withdrawalInput.clear(); }
};

//PaymentMenuScreen: User selects payment options.
class PaymentMenuScreen : public ScreenBase {
public:
    void draw(ATM& atm) override {
        atm.drawCenteredText("PAYMENT / PURCHASE", 40, 36, atm.resources.heading);
        atm.drawMenuBoxes(atm.paymentItems, atm.paymentSelection, 200, 38, 16, 90, 22);
        atm.drawCenteredText("\nUse UP/DOWN arrows or click, ENTER to select", 320, 17, atm.resources.infoC);
    }
    void handleEvent(ATM& atm, const sf::Event& event) override {
        if (event.type == sf::Event::KeyPressed) {
            if (event.key.code == sf::Keyboard::Down)
                atm.paymentSelection = (atm.paymentSelection + 1) % atm.paymentItems.size();
            else if (event.key.code == sf::Keyboard::Up)
                atm.paymentSelection = (atm.paymentSelection + atm.paymentItems.size() - 1) % atm.paymentItems.size();
            else if (event.key.code == sf::Keyboard::Enter) {
                openMenu(atm, atm.paymentSelection);
            }
        }
        else if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
            sf::Vector2i pixelPos = { event.mouseButton.x, event.mouseButton.y };
            sf::Vector2f worldPos = atm.window.mapPixelToCoords(pixelPos);
            int clicked = atm.getMenuBoxClicked(atm.paymentItems, 200, 38, 16, 90, worldPos);
            if (clicked != -1) {
                atm.paymentSelection = clicked;
                openMenu(atm, clicked);
            }
        }
    }
    static void openMenu(ATM& atm, int idx) {
        switch (idx) {
        case 0: atm.mobileTopupInput.clear(); atm.switchScreen("mobiletopup"); break;
        case 1: atm.customerNoInput.clear(); atm.billAmountInput.clear(); atm.switchScreen("electricitybill"); break;
        case 2: atm.customerNoInput.clear(); atm.billAmountInput.clear(); atm.switchScreen("waterbill"); break;
        case 3: atm.switchScreen("mainmenu"); break;
        }
    }
    void onEnter(ATM& atm) override { atm.paymentSelection = 0; }
};

// MobileTopupScreen: Handles mobile topup transactions.
class MobileTopupScreen : public ScreenBase {
public:
    void draw(ATM& atm) override {
        atm.drawCenteredText("MOBILE TOPUP", 70, 30, atm.resources.heading);
        atm.drawCenteredText("Enter Mobile Number:", 120, 22, sf::Color::White);
        atm.drawInputBox(atm.mobileTopupInput, 650.f / 2 - 110, 150, 220, 38, false);
        atm.drawCenteredText("Press ENTER to Topup, ESC to menu", 210, 16, atm.resources.infoC);
        if (!atm.errorMsg.empty())
            atm.drawCenteredText(atm.errorMsg, 250, 16, atm.resources.errorC);
    }
    void handleEvent(ATM& atm, const sf::Event& event) override {
        if (event.type == sf::Event::TextEntered) {
            if (event.text.unicode < 128) {
                char ch = static_cast<char>(event.text.unicode);
                if (ch >= '0' && ch <= '9' && atm.mobileTopupInput.size() < 12)
                    atm.mobileTopupInput += ch;
                if (ch == '\b' && !atm.mobileTopupInput.empty())
                    atm.mobileTopupInput.pop_back();
            }
        }
        if (event.type == sf::Event::KeyPressed) {
            if (event.key.code == sf::Keyboard::Escape) atm.switchScreen("paymentmenu");
            if (event.key.code == sf::Keyboard::Enter) {
                if (atm.mobileTopupInput.size() < 10) { atm.showError("Invalid number!", "mobiletopup"); return; }
                double amt = 100;
                if (atm.currentAccount->balance < amt) { atm.showError("Insufficient balance!", "mobiletopup"); return; }
                atm.currentAccount->balance -= amt;
                atm.saveAccountsToFile();
                atm.successMsg = "Topup successful! Rs 100 deducted.";
                atm.lastSuccessType = ATM::Topup;
                atm.playSound(atm.lastSuccessType);
                atm.switchScreen("success");
            }
        }
    }
    void onEnter(ATM& atm) override { atm.mobileTopupInput.clear(); }
};

//BillScreen: Handles electricity and water bill payments.
class BillScreen : public ScreenBase {
    std::string billType;
public:
    BillScreen(const std::string& type) : billType(type) {}
    void draw(ATM& atm) override {
        std::string title = (billType == "Electricity") ? "ELECTRICITY BILL" : "WATER BILL";
        atm.drawCenteredText(title, 70, 30, atm.resources.heading);
        atm.drawCenteredText("Customer Number (5 digits):", 120, 22, sf::Color::White);
        atm.drawInputBox(atm.customerNoInput, 650.f / 2 - 110, 145, 220, 38, false);
        atm.drawCenteredText("Amount:", 190, 22, sf::Color::White);
        atm.drawInputBox(atm.billAmountInput, 650.f / 2 - 110, 215, 220, 38, false);
        atm.drawCenteredText("Enter to Pay, ESC to menu", 270, 16, atm.resources.infoC);
        if (!atm.errorMsg.empty())
            atm.drawCenteredText(atm.errorMsg, 315, 16, atm.resources.errorC);
    }
    void handleEvent(ATM& atm, const sf::Event& event) override {
        if (event.type == sf::Event::TextEntered) {
            if (event.text.unicode < 128) {
                char ch = static_cast<char>(event.text.unicode);
                if (ch >= '0' && ch <= '9') {
                    if (atm.customerNoInput.size() < 5 && atm.billAmountInput.empty())
                        atm.customerNoInput += ch;
                    else if (atm.billAmountInput.size() < 8)
                        atm.billAmountInput += ch;
                }
                if (ch == '\b') {
                    if (!atm.billAmountInput.empty())
                        atm.billAmountInput.pop_back();
                    else if (!atm.customerNoInput.empty())
                        atm.customerNoInput.pop_back();
                }
            }
        }
        if (event.type == sf::Event::KeyPressed) {
            if (event.key.code == sf::Keyboard::Escape) atm.switchScreen("paymentmenu");
            if (event.key.code == sf::Keyboard::Enter) {
                if (atm.customerNoInput.size() != 5 || atm.billAmountInput.empty()) { atm.showError("Customer Number must be 5 digits!", billType == "Electricity" ? "electricitybill" : "waterbill"); return; }
                double amt = std::stod(atm.billAmountInput);
                if (amt < 50) { atm.showError("Min amount 50!", billType == "Electricity" ? "electricitybill" : "waterbill"); return; }
                if (amt > atm.currentAccount->balance) { atm.showError("Insufficient balance!", billType == "Electricity" ? "electricitybill" : "waterbill"); return; }
                atm.currentAccount->balance -= amt;
                atm.saveAccountsToFile();
                atm.successMsg = "Bill paid successfully!";
                atm.lastSuccessType = ATM::Bill;
                atm.playSound(atm.lastSuccessType);
                atm.switchScreen("success");
            }
        }
    }
    void onEnter(ATM& atm) override { atm.customerNoInput.clear(); atm.billAmountInput.clear(); }
};

//OtherMenuScreen: Displays other transactions like balance, transfer, change PIN.
class OtherMenuScreen : public ScreenBase {
public:
    void draw(ATM& atm) override {
        atm.drawCenteredText("OTHER TRANSACTIONS", 40, 36, atm.resources.heading);
        atm.drawMenuBoxes(atm.otherItems, atm.otherMenuSelection, 200, 38, 16, 90, 22);
        atm.drawCenteredText("Use UP/DOWN arrows or click, ENTER to select", 320, 17, atm.resources.infoC);
    }
    void handleEvent(ATM& atm, const sf::Event& event) override {
        if (event.type == sf::Event::KeyPressed) {
            if (event.key.code == sf::Keyboard::Down)
                atm.otherMenuSelection = (atm.otherMenuSelection + 1) % atm.otherItems.size();
            else if (event.key.code == sf::Keyboard::Up)
                atm.otherMenuSelection = (atm.otherMenuSelection + atm.otherItems.size() - 1) % atm.otherItems.size();
            else if (event.key.code == sf::Keyboard::Enter) {
                openMenu(atm, atm.otherMenuSelection);
            }
        }
        else if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
            sf::Vector2i pixelPos = { event.mouseButton.x, event.mouseButton.y };
            sf::Vector2f worldPos = atm.window.mapPixelToCoords(pixelPos);
            int clicked = atm.getMenuBoxClicked(atm.otherItems, 200, 38, 16, 90, worldPos);
            if (clicked != -1) {
                atm.otherMenuSelection = clicked;
                openMenu(atm, clicked);
            }
        }
    }
    static void openMenu(ATM& atm, int idx) {
        switch (idx) {
        case 0: atm.switchScreen("balance"); break;
        case 1: atm.transferAccountInput.clear(); atm.transferAmountInput.clear(); atm.switchScreen("transfer"); break;
        case 2: atm.newPinInput.clear(); atm.switchScreen("changepin"); break;
        case 3: atm.switchScreen("mainmenu"); break;
        }
    }
    void onEnter(ATM& atm) override { atm.otherMenuSelection = 0; }
};

//BlanceScreen: Displays user's account balance.
class BalanceScreen : public ScreenBase {
public:
    void draw(ATM& atm) override {
        atm.drawCenteredText("YOUR ACCOUNT BALANCE", 120, 32, atm.resources.heading);
        std::ostringstream oss;
        oss << "Rs " << std::fixed << std::setprecision(2) << atm.currentAccount->balance;
        atm.drawCenteredText(oss.str(), 170, 30, sf::Color::Yellow);
        atm.drawCenteredText("Press ENTER to continue", 250, 18, atm.resources.infoC);
    }
    void handleEvent(ATM& atm, const sf::Event& event) override {
        if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Enter)
            atm.switchScreen("mainmenu");
    }
};

//TransferScreen: Allows user to transfer money to another account.
class TransferScreen : public ScreenBase {
public:
    void draw(ATM& atm) override {
        atm.drawCenteredText("TRANSFER", 60, 30, atm.resources.heading);
        atm.drawCenteredText("Recipient User ID ", 110, 22, sf::Color::White);
        atm.drawInputBox(atm.transferAccountInput, 650.f / 2 - 110, 135, 220, 38, false);
        atm.drawCenteredText("Amount (min 1000):", 190, 22, sf::Color::White);
        atm.drawInputBox(atm.transferAmountInput, 650.f / 2 - 110, 215, 220, 38, false);
        atm.drawCenteredText("\nTAB to switch, Enter to Transfer, ESC to menu", 275, 16, atm.resources.infoC);
        // Focus indicator
        sf::RectangleShape focusRect(sf::Vector2f(224, 42));
        focusRect.setOutlineThickness(3);
        focusRect.setOutlineColor(atm.resources.menuSelect);
        focusRect.setFillColor(sf::Color::Transparent);
        focusRect.setPosition(650.f / 2 - 112, atm.transferAccountInputFocused ? 133 : 213);
        atm.window.draw(focusRect);
        if (!atm.errorMsg.empty())
            atm.drawCenteredText(atm.errorMsg, 320, 16, atm.resources.errorC);
    }
    void handleEvent(ATM& atm, const sf::Event& event) override {
        if (event.type == sf::Event::TextEntered) {
            if (event.text.unicode < 128) {
                char ch = static_cast<char>(event.text.unicode);
                if (atm.transferAccountInputFocused) {
                    if (ch >= '0' && ch <= '9' && atm.transferAccountInput.size() < 12)
                        atm.transferAccountInput += ch;
                    if (ch == '\b' && !atm.transferAccountInput.empty())
                        atm.transferAccountInput.pop_back();
                }
                else {
                    if (ch >= '0' && ch <= '9' && atm.transferAmountInput.size() < 9)
                        atm.transferAmountInput += ch;
                    if (ch == '\b' && !atm.transferAmountInput.empty())
                        atm.transferAmountInput.pop_back();
                }
            }
        }
        if (event.type == sf::Event::KeyPressed) {
            if (event.key.code == sf::Keyboard::Tab) {
                atm.transferAccountInputFocused = !atm.transferAccountInputFocused;
            }
            if (event.key.code == sf::Keyboard::Escape) atm.switchScreen("othermenu");
            if (event.key.code == sf::Keyboard::Enter) {
                if (atm.transferAccountInputFocused) {
                    atm.transferAccountInputFocused = false;
                    return;
                }
                // Validation
                if (atm.transferAccountInput.empty() || atm.transferAmountInput.empty()) { atm.showError("Recipient User ID and Amount cannot be empty!", "transfer"); return; }
                if (atm.transferAccountInput.size() > 12) { atm.showError("Recipient User ID max 12 digits!", "transfer"); return; }
                double amt = std::stod(atm.transferAmountInput);
                if (amt < 1000) { atm.showError("Minimum transfer is 1000!", "transfer"); return; }
                if (amt > atm.currentAccount->balance) { atm.showError("Insufficient balance!", "transfer"); return; }
                bool found = false;
                for (auto& acc : atm.accounts) {
                    if (acc.userID == atm.transferAccountInput) {
                        if (&acc == atm.currentAccount) {
                            atm.showError("Cannot transfer to your own account!", "transfer");
                            return;
                        }
                        acc.balance += amt;
                        atm.currentAccount->balance -= amt;
                        found = true;
                        atm.saveAccountsToFile();
                        atm.successMsg = "Transfer successful!";
                        atm.lastSuccessType = ATM::Transfer;
                        atm.playSound(atm.lastSuccessType);
                        atm.switchScreen("success");
                        break;
                    }
                }
                if (!found) { atm.showError("Recipient User ID not found!", "transfer"); }
            }
        }
    }
    void onEnter(ATM& atm) override {
        atm.transferAccountInput.clear();
        atm.transferAmountInput.clear();
        atm.transferAccountInputFocused = true;
    }
};

//ChangePinScreen: Allows user to change their PIN.
class ChangePinScreen : public ScreenBase {
public:
    void draw(ATM& atm) override {
        atm.drawCenteredText("CHANGE PIN", 100, 30, atm.resources.heading);
        atm.drawCenteredText("Enter new PIN (6 digits):", 160, 22, sf::Color::White);
        atm.drawInputBox(atm.newPinInput, 650.f / 2 - 110, 195, 220, 38, true);
        atm.drawCenteredText("Press ENTER to change, ESC for menu", 250, 16, atm.resources.infoC);
        if (!atm.errorMsg.empty())
            atm.drawCenteredText(atm.errorMsg, 300, 16, atm.resources.errorC);
    }
    void handleEvent(ATM& atm, const sf::Event& event) override {
        if (event.type == sf::Event::TextEntered) {
            if (event.text.unicode < 128) {
                char ch = static_cast<char>(event.text.unicode);
                if (ch >= '0' && ch <= '9' && atm.newPinInput.size() < 6)
                    atm.newPinInput += ch;
                if (ch == '\b' && !atm.newPinInput.empty())
                    atm.newPinInput.pop_back();
            }
        }
        if (event.type == sf::Event::KeyPressed) {
            if (event.key.code == sf::Keyboard::Escape) atm.switchScreen("othermenu");
            if (event.key.code == sf::Keyboard::Enter) {
                if (atm.newPinInput.size() != 6) { atm.showError("PIN must be 6 digits!", "changepin"); return; }
                atm.currentAccount->pinHash = Util::sha256(atm.newPinInput);
                atm.saveAccountsToFile();
                atm.successMsg = "PIN changed successfully!";
                atm.lastSuccessType = ATM::Pin;
                atm.playSound(atm.lastSuccessType);
                atm.switchScreen("success");
            }
        }
    }
    void onEnter(ATM& atm) override { atm.newPinInput.clear(); }
};

//SuccessScreen: Displays success messages after transactions.
class SuccessScreen : public ScreenBase {
public:
    void draw(ATM& atm) override {
        atm.drawCenteredText("SUCCESS!", 70, 38, atm.resources.successC);
        switch (atm.lastSuccessType) {
        case ATM::Cash:
            if (atm.resources.cashSuccessTexture.getSize().x > 0)
                atm.window.draw(atm.resources.cashSuccessSprite);
            break;
        case ATM::Pin:
            if (atm.resources.pinSuccessTexture.getSize().x > 0)
                atm.window.draw(atm.resources.pinSuccessSprite);
            break;
        case ATM::Topup:
            if (atm.resources.topupSuccessTexture.getSize().x > 0)
                atm.window.draw(atm.resources.topupSuccessSprite);
            break;
        case ATM::Bill:
            if (atm.resources.billSuccessTexture.getSize().x > 0)
                atm.window.draw(atm.resources.billSuccessSprite);
            break;
        case ATM::Transfer:
            if (atm.resources.transferSuccessTexture.getSize().x > 0)
                atm.window.draw(atm.resources.transferSuccessSprite);
            break;
        default: break;
        }
        atm.drawCenteredText(atm.successMsg, 220, 21, sf::Color::White);
        atm.drawCenteredText("\n\n\n\n\nPress ENTER to continue", 300, 17, sf::Color(150, 255, 150));
    }
    void handleEvent(ATM& atm, const sf::Event& event) override {
        if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Enter) {
            atm.switchScreen("mainmenu");
        }
    }
    void onEnter(ATM& atm) override { atm.successMsg.clear(); }
};

//ErrorScreen: Displays error messages.
class ErrorScreen : public ScreenBase {
public:
    void draw(ATM& atm) override {
        atm.drawCenteredText("ERROR", 70, 38, atm.resources.errorC);
        if (atm.resources.errorTexture.getSize().x > 0) atm.window.draw(atm.resources.errorSprite);
        atm.drawCenteredText(atm.errorMsg, 220, 21, sf::Color::White);
        atm.drawCenteredText("Press ENTER to return", 300, 17, sf::Color(255, 150, 150));
    }
    void handleEvent(ATM& atm, const sf::Event& event) override {
        if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Enter) {
            // Return to originating screen
            if (!atm.errorReturnScreen.empty()) {
                atm.switchScreen(atm.errorReturnScreen);
            }
            else if (atm.currentAccount) {
                atm.switchScreen("mainmenu");
            }
            else {
                atm.switchScreen("login");
            }
        }
    }
    void onEnter(ATM& atm) override { atm.playSound(ATM::Error); }
};

// Setup Screens
//Registers all screens with ATM.
void ATM::setupScreens() {
    screens["login"] = std::make_unique<LoginScreen>();
    screens["pin"] = std::make_unique<PinScreen>();
    screens["mainmenu"] = std::make_unique<MainMenuScreen>();
    screens["quickdraw"] = std::make_unique<QuickDrawScreen>();
    screens["withdrawal"] = std::make_unique<WithdrawalScreen>();
    screens["paymentmenu"] = std::make_unique<PaymentMenuScreen>();
    screens["mobiletopup"] = std::make_unique<MobileTopupScreen>();
    screens["electricitybill"] = std::make_unique<BillScreen>("Electricity");
    screens["waterbill"] = std::make_unique<BillScreen>("Water");
    screens["othermenu"] = std::make_unique<OtherMenuScreen>();
    screens["balance"] = std::make_unique<BalanceScreen>();
    screens["transfer"] = std::make_unique<TransferScreen>();
    screens["changepin"] = std::make_unique<ChangePinScreen>();
    screens["success"] = std::make_unique<SuccessScreen>();
    screens["error"] = std::make_unique<ErrorScreen>();
}

//Main function to run the ATM application.
int main() {
    ATM atm;
    atm.run();
    return 0;
}