
import hashlib
from re import findall
import requests
from bs4 import BeautifulSoup
from abc import ABC, abstractmethod
DEF_REST_TIMEOUT = 20
class PDU(ABC):
    """
    Base interface for Power Distribution Unit (PDU).
    Args:
        **kwargs: Keyword arguments passed to the class.
            ip_address (str): Required, IP address of the PDU.
            user_name (str): Required, Username used for the PDU account.
            password (str): Required, Password for the PDU account.
    Example:
        pdu = PDU(user_name='my_user', ip_address='192.168.231.122', password='my_password')
    Raises:
        ValueError: If 'user_name',password or 'ip_address' is not provided.
    """
    def __init__(self, **kwargs):
        self.user = kwargs.pop('user_name', None)
        self.ip = kwargs.pop('ip_address', None)
        self.password = kwargs.pop('password', None)
        if self.user is None or self.ip is None or self.password is None:
            raise ValueError("'user_name', 'password' and 'ip address' are required to create PDU.")
        self.disabled_outlet = {}
        self.outlet_index_map = None
        self.outlets = {}
    @abstractmethod
    def turn_off_outlet(self, outlet_index:str) -> bool:
        pass
    @abstractmethod
    def turn_on_outlet(self, outlet_index:str) -> bool:
        pass
    @abstractmethod
    def cycle_outlet(self, outlet_index:str) -> bool:
        pass
class LPC(PDU):
    """ Pro Switch
    LPC Remote Power switch. Made by Digital Loggers Inc (https://www.digital-loggers.com/lpc.html)
    Args:
        **kwargs:
            ip_address (str): Required, IP address of the PDU.
            user_name (str): Required, Username used for the PDU account.
            password (str, optional): Password. Defaults to username if not provided.
    Example::
        pdu = LPC(user_name='boron', ip_address='192.168.231.122')
    """
    def __init__(self,**kwargs):
        if kwargs.get('password', None) is None:
            kwargs['password'] = kwargs.get('user_name', None)
        super().__init__(**kwargs)
        self.host = f'http://{self.ip}/'
        self.cookies = None
        self.login()
    def login(self) -> None:
        '''
            Login to the LCP and set the cookie in the LPC Object
        '''
        get_res = requests.get(self.host, timeout=DEF_REST_TIMEOUT)
        matches = findall('name="Challenge" value=".*"', get_res.text)
        if matches:
            # extract challenge value and calculate md5
            challenge_value = matches[0].split('=')[2][1:-1]
            h = hashlib.new('md5', bytearray(challenge_value+self.user+self.password+challenge_value, 'utf-8'))
            remote_power_password = h.hexdigest()
            post_res = requests.post(self.host+'login.tgi', data={'Username':self.user,'Password': remote_power_password},
                                     timeout=DEF_REST_TIMEOUT)
            if post_res.status_code == 200:
                self.cookies = post_res.cookies
                print(f'Logged into Remote Power at {self.host} with user: {self.user}')
            else:
                print(f'Error: Remote Power login failed. Response code: {post_res.status_code}')
    def logged_in(self) -> bool:
        '''
        Check if the object is currently logged into the LPC.
        Returns:
            Bool: Returns True if we are already logged into the LPC, else False
        '''
        return self.cookies is not None
    def logout(self) -> None:
        '''
            Log out of the LCP and delete the cookie in the LPC Object
        '''
        if not self.logged_in():
            return
        requests.get(self.host+'logout', cookies=self.cookies)
        self.cookies = None

    def cycle_outlet(self, outlet_index:str) -> bool:
        '''
        Power Cycle the given outlet_index.
        Args:
            String: Outlet index
        Returns:
            Bool: Returns True if we were able to power cycle the outlet at the give index else
            return False
        '''
        if not self.logged_in():
            print("Error: you must be logged in to cycle the outlet")
            return False
        get_res = requests.get(self.host+f'outlet?{outlet_index}=CCL', cookies=self.cookies, timeout=DEF_REST_TIMEOUT)
        if get_res.status_code == 200:
            return True
        else:
            return False
    def turn_off_outlet(self, outlet_index:str) -> bool:
        '''
        Turn off the outlet at the given outlet_index.
        Args:
            String: Outlet index
        Returns:
            Bool: Returns True if we were able to power off the outlet at the give index else
            return False
        '''
        if not self.logged_in():
            print("Error: you must be logged in to turn off the outlet")
            return False
        get_res = requests.get(self.host+f'outlet?{outlet_index}=OFF', cookies=self.cookies, timeout=DEF_REST_TIMEOUT)
        if get_res.status_code == 200:
            return True
        else:
            return False
    def turn_on_outlet(self, outlet_index:str) -> bool:
        '''
        Power on the outlet at the given outlet_index.
        Args:
            String: Outlet index
        Returns:
            Bool: Returns True if we were able to power off the outlet at the give index else
            return False
        '''
        if not self.logged_in():
            print("Error: you must be logged in to turn on the outlet")
            return False
        get_res = requests.get(self.host+f'outlet?{outlet_index}=ON', cookies=self.cookies, timeout=DEF_REST_TIMEOUT)
        if get_res.status_code == 200:
            return True
        else:
            return False