#include <string>
#include <map>
#include <vector>

class UniValue;

using namespace std;

class Exch {
  public:
  Exch(const string &retAddr); 
  virtual ~Exch() {}

  virtual const string& Name() const = 0;
  virtual const string& Host() const = 0;
  
  // Get currency for exchnagge to, like btc, ltc, etc
  // Fill MarketInfo from exchange.
  // Returns the empty string if OK, or error message, if error
  virtual string MarketInfo(const string &currency, double amount) = 0;

  // Create SEND exchange channel for 
  // Send "amount" in external currecny "to" address
  // Fills m_depAddr..m_txKey, and updates m_rate
  // Returns error text, or an empty string, if OK
  virtual string Send(const string &to, double amount) = 0;

  // Check status of existing transaction.
  // If key is empty, used the last key
  // Returns status (including err), or minus "-", if "not my" key
  virtual string TxStat(const string &txkey, UniValue &details) = 0;

  // Cancel TX by txkey.
  // If key is empty, used the last key
  // Returns error text, or an empty string, if OK
  // Returns minus "-", if "not my" key
  virtual string CancelTX(const string &txkey) = 0;

  // Check time in secs, remain in the contract, created by prev Send()
  // If key is empty, used the last key
  // Returns time or zero, if contract expired
  // Returns -1, if "not my" key
  virtual int Remain(const string &txkey) = 0;

  // Returns extimated MFC to pay for specific pay_amount
  // Must be called after MarketInfo
  virtual double EstimatedMFC(double pay_amount) const;

  // Adjust amount for un-precise exchange calculation and market fluctuations
  // by adding add_percent (default) or config param "adjexchEXCHANGE_NAME"
  double AdjustExchAmount(double amo, double add_percent) const;

  bool _sandBox = false;
  string m_retAddr; // Return MFC Addr

  // MarketInfo fills these params
  string m_pair;
  double m_rate;        // $MFC/$BTC
  double m_limit;       // BTC
  double m_min;         // BTC
  double m_minerFee;    // BTC

  // Send fills these params + m_rate above
  string m_depAddr;	// Address to pay MFC
  string m_outAddr;	// Address to pay from exchange
  double m_depAmo;	// amount in MFC
  double m_outAmo;	// Amount transferred to BTC
  string m_txKey;	// TX reference key

  protected:
  // Fill exchange-specific fields into m_header for future https request
  virtual void FillHeader() {}
  // Connect to the server by https, fetch JSON and parse to UniValue
  // Throws exception if error
  UniValue httpsFetch(const char *get, const UniValue *post);

  // Get input path within server, like: /api/marketinfo/mfc_btc.json
  // Called from exchange-specific MarketInfo()
  // Fill MarketInfo from exchange.
  // Throws exception if error
  const UniValue RawMarketInfo(const string &path);

  // Check JSON-answer for "error" key, and throw error
  // message, if exists
  virtual void CheckERR(const UniValue &reply) const;

  // Extract raw key from txkey
  // Return NULL if "Not my key" or invalid key
  const char *RawKey(const string &txkey) const;

  // extra header fields, needed for some exchange
  std::map<std::string,std::string> m_header;

}; // class Exch

//-----------------------------------------------------
class ExchCoinReform : public Exch {
  public:
  ExchCoinReform(const string &retAddr);

  virtual const string& Name() const override;
  virtual const string& Host() const override;

  // Get currency for exchnagge to, like btc, ltc, etc
  // Fill MarketInfo from exchange.
  // Returns the empty string if OK, or error message, if error
  virtual string MarketInfo(const string &currency, double amount)override;

  // Creatse SEND exchange channel for 
  // Send "amount" in external currecny "to" address
  // Fills m_depAddr..m_txKey, and updates m_rate
  virtual string Send(const string &to, double amount)override;

  // Check status of existing transaction.
  // If key is empty, used the last key
  // Returns status (including err), or minus "-", if "not my" key
  virtual string TxStat(const string &txkey, UniValue &details)override;

  // If key is empty, used the last key
  // Returns error text, or an empty string, if OK
  // Returns minus "-", if "not my" key
  virtual string CancelTX(const string &txkey)override;

  // Check time in secs, remain in the contract, created by prev Send()
  // If key is empty, used the last key
  // Returns time or zero, if contract expired
  // Returns -1, if "not my" key
  virtual int Remain(const string &txkey)override;

}; // class ExchCoinReform

// See https://developer.coinswitch.co
class ExchCoinSwitch : public Exch {
  public:
  ExchCoinSwitch(const string &retAddr);

  virtual const string& Name() const override;
  virtual const string& Host() const override;

  // Get currency for exchnagge to, like btc, ltc, etc
  // Fill MarketInfo from exchange.
  // Returns the empty string if OK, or error message, if error
  virtual string MarketInfo(const string &currency, double amount)override;

  // Creatse SEND exchange channel for 
  // Send "amount" in external currecny "to" address
  // Fills m_depAddr..m_txKey, and updates m_rate
  virtual string Send(const string &to, double amount)override;

  // Check status of existing transaction.
  // If key is empty, used the last key
  // Returns status (including err), or minus "-", if "not my" key
  virtual string TxStat(const string &txkey, UniValue &details)override;

  // Cancel TX by txkey.
  // If key is empty, used the last key
  // Returns error text, or an empty string, if OK
  // Returns minus "-", if "not my" key
  virtual string CancelTX(const string &txkey)override;

  // Check time in secs, remain in the contract, created by prev Send()
  // If key is empty, used the last key
  // Returns time or zero, if contract expired
  // Returns -1, if "not my" key
  virtual int Remain(const string &txkey)override;

  private:
  // Fill exchange-specific fields into m_header for future https request
  virtual void FillHeader()override;
  // Check JSON-answer for "error" key, and throw error message, if exists
  virtual void CheckERR(const UniValue &reply) const;

}; // class ExchCoinSwitch


// See https://easyrabbit.net/
class ExchEasyRabbit : public Exch {
  public:
  ExchEasyRabbit(const string &retAddr);

  virtual const string& Name() const override;
  virtual const string& Host() const override;

  // Get currency for exchnagge to, like btc, ltc, etc
  // Fill MarketInfo from exchange.
  // Returns the empty string if OK, or error message, if error
  virtual string MarketInfo(const string &currency, double amount)override;

  // Returns extimated MFC to pay for specific pay_amount
  // Must be called after MarketInfo
  virtual double EstimatedMFC(double pay_amount) const;

  // Creatse SEND exchange channel for
  // Send "amount" in external currecny "to" address
  // Fills m_depAddr..m_txKey, and updates m_rate
  virtual string Send(const string &to, double amount)override;

  // Check status of existing transaction.
  // If key is empty, used the last key
  // Returns status (including err), or minus "-", if "not my" key
  virtual string TxStat(const string &txkey, UniValue &details)override;

  // Cancel TX by txkey.
  // If key is empty, used the last key
  // Returns error text, or an empty string, if OK
  // Returns minus "-", if "not my" key
  virtual string CancelTX(const string &txkey)override;

  // Check time in secs, remain in the contract, created by prev Send()
  // If key is empty, used the last key
  // Returns time or zero, if contract expired
  // Returns -1, if "not my" key
  virtual int Remain(const string &txkey)override;

  private:
  // Fill exchange-specific fields into m_header for future https request
//  virtual void FillHeader()override;
  // Check JSON-answer for "error" key, and throw error message, if exists
  virtual void CheckERR(const UniValue &reply) const;

}; // class ExchEasyRabbit





//-----------------------------------------------------
class ExchBox {
  public:
  ExchBox();
  ~ExchBox();
  void Reset(const string &retAddr);

  vector<Exch*> m_v_exch;
}; // class exchbox

//-----------------------------------------------------
