
DROP DATABASE IF EXISTS boost_mysql_oms;
CREATE DATABASE boost_mysql_oms;
USE boost_mysql_oms;

CREATE TABLE users (
    id INT PRIMARY KEY AUTO_INCREMENT,
    first_name VARCHAR(50) NOT NULL,
    last_name VARCHAR(50) NOT NULL
);

CREATE TABLE products (
    id INT PRIMARY KEY AUTO_INCREMENT,
    short_name VARCHAR(100) NOT NULL,
    descr TEXT DEFAULT '',
    price INT NOT NULL,
    FULLTEXT(short_name, descr)
);

CREATE TABLE orders(
    id INT PRIMARY KEY AUTO_INCREMENT,
    user_id INT NOT NULL,
    `status` ENUM('draft', 'pending_payment', 'complete') NOT NULL DEFAULT 'draft',
    FOREIGN KEY (user_id) REFERENCES users(id)
);

CREATE TABLE order_items(
    id INT PRIMARY KEY AUTO_INCREMENT,
    order_id INT NOT NULL,
    product_id INT NOT NULL,
    quantity INT NOT NULL,
    FOREIGN KEY (order_id) REFERENCES orders(id),
    FOREIGN KEY (product_id) REFERENCES products(id)
);

DELIMITER //

CREATE PROCEDURE get_products(IN p_search VARCHAR(50))
BEGIN
    DECLARE max_products INT DEFAULT 20;
    IF p_search IS NULL THEN
        SELECT id, short_name, descr, price
        FROM products
        LIMIT max_products;
    ELSE
        SELECT id, short_name, descr, price FROM products
        WHERE MATCH(short_name, descr) AGAINST(p_search)
        LIMIT max_products;
    END IF;
END //

CREATE PROCEDURE create_order(IN p_user_id INT)
BEGIN
    DECLARE actual_id INT;
    START TRANSACTION;

    -- Check parameters
    IF p_user_id IS NULL THEN
        SIGNAL SQLSTATE '45000' SET MYSQL_ERRNO = 1048, MESSAGE_TEXT = 'create_order: invalid params';
    END IF;
    SELECT id FROM users WHERE id = p_user_id INTO actual_id;
    IF actual_id IS NULL THEN
        SIGNAL SQLSTATE '45000' SET MYSQL_ERRNO = 1329, MESSAGE_TEXT = 'The given user does not exist';
    END IF;

    -- Actually create the order
    INSERT INTO orders (user_id) VALUES (p_user_id);

    -- Return the created order
    SELECT id, user_id, `status`
    FROM orders WHERE id = order_id;

    COMMIT;
END //

CREATE PROCEDURE add_line_item(
    IN p_user_id INT,
    IN p_order_id INT,
    IN p_product_id INT,
    IN p_quantity INT,
    OUT pout_line_item_id INT
)
BEGIN
    DECLARE order_status TEXT;
    START TRANSACTION;

    -- Check parameters
    IF p_order_id IS NULL OR p_user_id IS NULL OR p_product_id IS NULL OR p_quantity IS NULL OR p_quantity <= 0 THEN
        SIGNAL SQLSTATE '45000' SET MYSQL_ERRNO = 1048, MESSAGE_TEXT = 'add_line_item: invalid params';
    END IF;

    -- Ensure that the product is valid
    SELECT price INTO product_price FROM products WHERE id = product_id;
    IF product_price IS NULL THEN
        SIGNAL SQLSTATE '45000' SET MYSQL_ERRNO = 1329, MESSAGE_TEXT = 'The given product does not exist';
    END IF;

    -- Get the order
    SELECT `status` INTO order_status FROM orders WHERE id = p_order_id AND user_id = p_user_id;
    IF order_status IS NULL THEN
        SIGNAL SQLSTATE '45000' SET MYSQL_ERRNO = 1329, MESSAGE_TEXT = 'The given order does not exist';
    END IF;
    IF order_status <> 'draft' THEN
        SIGNAL SQLSTATE '45000' SET MYSQL_ERRNO = 1000, MESSAGE_TEXT = 'The given is not editable';
    END IF;

    -- Insert the new item
    INSERT INTO order_items (order_id, product_id, quantity) VALUES (p_order_id, p_product_id, p_quantity);

    -- Return value
    SET pout_line_item_id = LAST_INSERT_ID();

    -- Return the edited order
    SELECT id, user_id, `status`
    FROM orders WHERE id = p_order_id;
    SELECT
        item.id AS item_id,
        item.quantity AS quantity,
        prod.price AS unit_price
    FROM order_items item
    JOIN products prod ON item.product_id = prod.id
    WHERE item.order_id = p_order_id;

    COMMIT;
END //

CREATE PROCEDURE remove_line_item(
    IN p_user_id,
    IN p_line_item_id INT
)
BEGIN
    DECLARE order_id INT;
    DECLARE order_status TEXT;
    START TRANSACTION;

    -- Check parameters
    IF p_user_id IS NULL OR p_order_id IS NULL OR p_line_item_id IS NULL THEN
        SIGNAL SQLSTATE '45000' SET MYSQL_ERRNO = 1048, MESSAGE_TEXT = 'remove_line_item: invalid params';
    END IF;

    -- Get the order
    SELECT id, `status`
    INTO order_id, order_status
    FROM orders
    JOIN order_items items ON (orders.id = items.order_id)
    WHERE items.id = p_line_item_id;

    IF order_status IS NULL THEN
        SIGNAL SQLSTATE '45000' SET MYSQL_ERRNO = 1329, MESSAGE_TEXT = 'The given order item does not exist';
    END IF;
    IF order_status <> 'draft' THEN
        SIGNAL SQLSTATE '45000' SET MYSQL_ERRNO = 1000, MESSAGE_TEXT = 'The given order is not editable';
    END IF;

    -- Delete the line item
    DELETE FROM order_items
    WHERE id = p_line_item_id;

    -- Return the edited order
    SELECT id, user_id, `status`
    FROM orders WHERE id = order_id;
    SELECT
        item.id AS item_id,
        item.quantity AS quantity,
        prod.price AS unit_price
    FROM order_items item
    JOIN products prod ON item.product_id = prod.id
    WHERE item.order_id = order_id;

    COMMIT;
END //

CREATE PROCEDURE checkout_order(
    IN order_id INT,
    OUT order_user_id INT,
    OUT order_status ENUM('draft', 'pending_payment'),
    OUT order_total INT
)
BEGIN
    START TRANSACTION;

    -- Check parameters
    IF order_id IS NULL THEN
        SIGNAL SQLSTATE '45000' SET MYSQL_ERRNO = 1048, MESSAGE_TEXT = 'order_id cannot be NULL';
    END IF;

    -- Get the order
    SELECT user_id, `status`
    INTO order_user_id, order_status
    FROM orders WHERE id = order_id;

    IF order_status IS NULL THEN
        SIGNAL SQLSTATE '45000' SET MYSQL_ERRNO = 1329, MESSAGE_TEXT = 'The given order_id does not exist';
    END IF;
    IF order_status <> 'draft' THEN
        SIGNAL SQLSTATE '45000' SET MYSQL_ERRNO = 1000, MESSAGE_TEXT = 'The given is not in a state that can be checked out';
    END IF;

    -- Update the order
    UPDATE orders SET `status` = 'pending_payment' WHERE id = order_id;

    -- Retrieve the total price
    SELECT SUM(prod.price * item.quantity)
    INTO order_total
    FROM order_items item
    JOIN products prod ON item.product_id = prod.id
    WHERE item.order_id = order_id;

    COMMIT;
END //

CREATE PROCEDURE get_order(
    IN order_id INT,
    OUT order_user_id INT,
    OUT order_status ENUM('draft', 'pending_payment')
)
BEGIN
    START TRANSACTION READ ONLY;

    -- Check parameters
    IF order_id IS NULL THEN
        SIGNAL SQLSTATE '45000' SET MYSQL_ERRNO = 1048, MESSAGE_TEXT = 'order_id cannot be NULL';
    END IF;

    -- Get the order
    SELECT user_id, `status`
    INTO order_user_id, order_status
    FROM orders
    WHERE id = order_id;
    
    IF order_user_id IS NULL THEN
        SIGNAL SQLSTATE '45000' SET MYSQL_ERRNO = 1329, MESSAGE_TEXT = 'The given order_id does not exist';
    END IF;

    -- Get the line items
    SELECT
        item.id AS item_id,
        item.quantity AS quantity,
        prod.price AS unit_price
    FROM order_items item
    JOIN products prod ON item.product_id = prod.id
    WHERE item.order_id = order_id;

    COMMIT;
END //

DELIMITER ;


INSERT INTO users (first_name, last_name) VALUES
    ('John', 'Doe'),
    ('Alex', 'Smith'),
    ('admin', 'admin')
;

INSERT INTO products (price, short_name, descr) VALUES
    (6400, 'A Feast for Odin', 'A Feast for Odin is a points-driven game, with plethora of pathways to victory, with a range of risk balanced against reward. A significant portion of this is your central hall, which has a whopping -86 points of squares and a major part of your game is attempting to cover these up with various tiles. Likewise, long halls and island colonies can also offer large rewards, but they will have penalties of their own.'),
    (1600, 'Railroad Ink',     'The critically acclaimed roll and write game where you draw routes on your board trying to connect the exits at its edges. The more you connect, the more points you make, but beware: each incomplete route will make you lose points!'),
    (4000, 'Catan',            'Catan is a board game for two to four players in which you compete to gather resources and build the biggest settlements on the fictional island of Catan. It takes approximately one hour to play.'),
    (2500, 'Not Alone',        'It is the 25th century. You are a member of an intergalactic expedition shipwrecked on a mysterious planet named Artemia. While waiting for the rescue ship, you begin to explore the planet but an alien entity picks up your scent and begins to hunt you. You are NOT ALONE! Will you survive the dangers of Artemia?'),
    (4500, 'Dice Hospital',    "In Dice Hospital, a worker placement board game, players are tasked with running a local hospital. Each round you'll be admitting new patients, hiring specialists, building new departments, and treating as many incoming patients as you can.")
;

CALL create_order(1, @order_id);
CALL add_line_item(@order_id, 1, 2, @item_id, @product_price);
CALL add_line_item(@order_id, 2, 1, @item_id, @product_price);
CALL get_order(@order_id, @dummy, @status);
CALL checkout_order(@order_id, @dummy, @status, @order_total);
SELECT @status, @order_total;
CALL get_order(@order_id, @dummy, @status);