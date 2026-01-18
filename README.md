# ATM Simulation System

A graphical ATM (Automated Teller Machine) simulation application built with C++ and SFML as part of the Object-Oriented Programming (OOP) course project.

## 📚 About the Project

This project was developed as part of the **Second Semester OOP Course** at **Pulchowk Campus, Institute of Engineering, Tribhuvan University**. 

### Team Members
- **Abishek Parajuli** [([@abishekparajuli-np](https://github.com/abishekparajuli-np))]
- **Aayush Dulal** [([@Aayushdulal](https://github.com/Aayushdulal))]
- **Aavash Sapkota**[([@sapkotaavash](https://github.com/sapkotaavash))]

## ✨ Features

- **User Authentication**: Secure login with User ID and PIN
- **PIN Security**: SHA256 hashing for secure PIN storage
- **Quick Cash**: Predefined withdrawal amounts (Rs. 1000 - Rs. 50,000)
- **Cash Withdrawal**: Custom amount withdrawal
- **Balance Inquiry**: Check account balance
- **Fund Transfer**: Transfer money to other accounts
- **Payment Services**:
  - Mobile Top-up
  - Electricity Bill Payment
  - Water Bill Payment
- **Change PIN**: Secure PIN modification
- **Graphical User Interface**: Built with SFML for an interactive experience
- **Sound Effects**: Audio feedback for transactions

## 🛠️ Technologies Used

- **Language**: C++
- **Graphics Library**: SFML (Simple and Fast Multimedia Library)
- **Encryption**: OpenSSL (SHA256)
- **IDE**: Visual Studio

## 📋 Prerequisites

Before running this project, make sure you have: 

- Visual Studio (2019 or later recommended)
- SFML Library (2.5.x or later)
- OpenSSL Library

## 🚀 Installation & Setup

1. **Clone the repository**
   ```bash
   git clone https://github.com/abishekparajuli-np/C-Project.git
   ```

2. **Open the solution**
   - Navigate to the `ATM_PROJECT` folder
   - Open `ATM_PROJECT.sln` in Visual Studio

3. **Configure SFML**
   - Link SFML libraries in project properties
   - Include SFML headers
   - Copy SFML DLLs to output directory

4. **Configure OpenSSL**
   - Link OpenSSL libraries
   - Include OpenSSL headers

5. **Build and Run**
   - Build the solution (Ctrl + Shift + B)
   - Run the application (F5)

## 📁 Project Structure

```
ATM_PROJECT/
├── assets/              # Images, sounds, and data files
├── main.cpp             # Main application code
├── test.cpp             # Test file
├── ATM_PROJECT. sln      # Visual Studio solution file
└── ATM_PROJECT.vcxproj  # Visual Studio project file
```

## 🎮 How to Use

1. Launch the application
2. Enter your User ID
3. Enter your PIN
4. Navigate through the menu using the interface: 
   - **Quick Cash**: Select predefined amounts
   - **Cash Withdrawal**:  Enter custom amount
   - **Payment/Purchase**:  Pay bills or mobile top-up
   - **Other Transactions**: Balance inquiry, transfer, change PIN
   - **Exit**:  Safely log out

## 🔒 Security Features

- PIN is hashed using SHA256 algorithm before storage
- Secure session management
- Input validation for all transactions

## 📸 Screenshots

*Add screenshots of your application here*

## 🎓 Academic Information

| Detail | Information |
|--------|-------------|
| Course | Object-Oriented Programming (OOP) |
| Semester | Second Semester |
| Institution | Pulchowk Campus, IOE, TU |
| Year | 2025 |

## 📄 License

This project is for educational purposes. 

## 🤝 Acknowledgments

- Pulchowk Campus, IOE for the learning opportunity
- SFML community for the excellent graphics library
- OpenSSL for cryptographic functions


