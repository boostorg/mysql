--
-- Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
--
-- Distributed under the Boost Software License, Version 1.0. (See accompanying
-- file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
--

-- Connection system variables
SET NAMES utf8;

-- Database
DROP DATABASE IF EXISTS boost_mysql_orders;
CREATE DATABASE boost_mysql_orders;
USE boost_mysql_orders;

-- User
DROP USER IF EXISTS 'orders_user'@'%';
CREATE USER 'orders_user'@'%' IDENTIFIED BY 'orders_password';
GRANT ALL PRIVILEGES ON boost_mysql_orders.* TO 'orders_user'@'%';
FLUSH PRIVILEGES;

-- Tables
CREATE TABLE products (
    id INT PRIMARY KEY AUTO_INCREMENT,
    short_name VARCHAR(100) NOT NULL,
    descr TEXT,
    price INT NOT NULL,
    FULLTEXT(short_name, descr)
);

CREATE TABLE orders(
    id INT PRIMARY KEY AUTO_INCREMENT,
    `status` ENUM('draft', 'pending_payment', 'complete') NOT NULL DEFAULT 'draft'
);

CREATE TABLE order_items(
    id INT PRIMARY KEY AUTO_INCREMENT,
    order_id INT NOT NULL,
    product_id INT NOT NULL,
    quantity INT NOT NULL,
    FOREIGN KEY (order_id) REFERENCES orders(id),
    FOREIGN KEY (product_id) REFERENCES products(id)
);

-- Contents for the products table
INSERT INTO products (price, short_name, descr) VALUES
    (6400, 'A Feast for Odin', 'A Feast for Odin is a points-driven game, with plethora of pathways to victory, with a range of risk balanced against reward. A significant portion of this is your central hall, which has a whopping -86 points of squares and a major part of your game is attempting to cover these up with various tiles. Likewise, long halls and island colonies can also offer large rewards, but they will have penalties of their own.'),
    (1600, 'Railroad Ink',     'The critically acclaimed roll and write game where you draw routes on your board trying to connect the exits at its edges. The more you connect, the more points you make, but beware: each incomplete route will make you lose points!'),
    (4000, 'Catan',            'Catan is a board game for two to four players in which you compete to gather resources and build the biggest settlements on the fictional island of Catan. It takes approximately one hour to play.'),
    (2500, 'Not Alone',        'It is the 25th century. You are a member of an intergalactic expedition shipwrecked on a mysterious planet named Artemia. While waiting for the rescue ship, you begin to explore the planet but an alien entity picks up your scent and begins to hunt you. You are NOT ALONE! Will you survive the dangers of Artemia?'),
    (4500, 'Dice Hospital',    "In Dice Hospital, a worker placement board game, players are tasked with running a local hospital. Each round you'll be admitting new patients, hiring specialists, building new departments, and treating as many incoming patients as you can.")
;
